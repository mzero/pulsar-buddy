
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   UTILITIES
//

const ttf = [true, true, false];
const ftt = [false, true, true];

// all measurements are in mm

function inch(i) { return 25.4 * i; }
function mils(m) { return 0.0254 * m; }


function colorize(c, o) {
    o = color(c, o);
    o.properties.color = c;
    return o;
}
function translucent(o) {
    c = o.properties.color ? o.properties.color : hsv2rgb(0, 0, 0.75);
    c = c.slice(0,3).concat(0.35);
    return colorize(c, o);
}

function unionInColor(o, a) {
    let c = o.properties.color;
    o = union(o, a);
    if (c)
        o = colorize(c, o);
    return o;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   EXTERNALS
//

// material: Bamboo Plywood
const t = 2.7;       // thickness
const k = 0.2;      // kerf

const pulsarH = 61.0;
    // table surface to top, including the rubber feet
    // Surface of top is at 63.0, but the back plate is a little shorter
    // this makes the unit line up with the back plate nicely.


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   BASIC BOXES
//

const defaultTab = 3*t;
const defaultFit = k/2;     // k was too much for bamboo, 0 too little
const sideTab = 1.5*t;
const longEdgeInset = mils(500);


function face(x, y) {
    const f = cube({ size: [x, y, t], center: true });
    const innerMark =
        translate(
            [-x/2+t, -y/2+t, -t/2],
            cube({ size: [2*t, 2*t, t/2] })
            );

    return difference(f, innerMark);
}


function boxParts(w, d, h, c) {
    const tbFace = face(w, d);
    const fbFace = face(w, h);
    const lrFace = face(h, d);

    const [ch, cs, cv] = rgb2hsv(c[0], c[1], c[2]);
    const ca = c.slice(3,4);

    return {
        tb: colorize(hsv2rgb(ch + 0/360, cs, cv * 1.00).concat(ca), tbFace),
        fb: colorize(hsv2rgb(ch + 5/360, cs, cv * 0.90).concat(ca), fbFace),
        lr: colorize(hsv2rgb(ch - 5/360, cs, cv * 0.80).concat(ca), lrFace)
    };
}

function tabJoin(objA, objB, orient, params, debug) {
    if (!objA || !objB) {
        return [objA, objB];
    }

    const joint = intersection(objA, objB);
    const [jmin, jmax] = joint.getBounds();

    const jmid = jmax.plus(jmin).dividedBy(2);
    const jsize = jmax.minus(jmin);

    const jlen = Math.max(jsize.x, jsize.y, jsize.z);  // hack!

    if (jlen < 0.01) {
        return [objA, objB];
    }

    const tab = (params && params.tab !== undefined) ? params.tab : defaultTab;
    const fit = (params && params.fit !== undefined) ? params.fit : defaultFit;
    const indent = (params && params.indent != undefined) ? params.indent : 0;

    var n = Math.floor((jlen - 2*indent)/tab);
    if (n%2 == 0) n -= 1;  // must be odd
    while ((jlen - n * tab)/2 < Math.max(2*t, 0*tab/2))
        n -= 2;     // make sure ends are reasonable
    const capx = (jlen - n*tab)/2;

    if (debug) console.log("len", jlen, "n", n, "capx", capx);


    // add back a prisim the size of the joint to both objects
    // this fills in any curves they might have that intersect the joint

    const jointBar = translate(jmin, cube({size: [jsize.x, jsize.y, jsize.z]}));
    objA = unionInColor(objA, jointBar);
    objB = unionInColor(objB, jointBar);

    // we'll, construct the knockouts along the x axis, and rotate them
    // into location later.

    var knockA = [];
    var knockB = [];
    var x = -jlen;  // start negative and work toward zero

    if (n > 0) {
        // only notch if there is room

        // first, knock out the capx from B;
        if (debug) console.log('notching', capx, 'on B');

        const firstNotch =
            translate([x + capx/2, 0, 0], cube({ size: [capx, t, t], center: true}));
        knockB.push(firstNotch);
        x += capx;

        const tabNotch = cube({ size: [tab - fit, t, t], center: true});

        // now alternate A/B cutting out a tab-kerf sized block every tab
        var onA = true;    // objA gets the next notch after the cap
        while (n > 0) {
            if (debug) console.log('notching', tab, 'on', onA ? 'A' : 'B');

            const notch = translate([x + tab/2, 0, 0], tabNotch);
            if (onA) knockA.push(notch);
            else     knockB.push(notch);

            n -= 1;
            x += tab;
            onA = !onA;
        }
    } else {
        // otherwise, do nothing, the whole A side will fit into the space,
        // and the B side will get notched for it
    }

    // last side gets the remaining notch
    if (debug) console.log('notching', -x, 'on', onA ? 'A' : 'B');

    const lastNotch =
        translate([x/2, 0, 0], cube({ size: [-x, t, t], center: true}));
    if (onA) knockA.push(lastNotch);
    else     knockB.push(lastNotch);

    // center the notch columns, rotate, then move to cover the joint
    // and then remove material from the objects

    if (knockA.length) {
        knockA = translate(jmid, rotate(orient, translate([jlen/2, 0, 0], knockA)));
        objA = difference(objA, knockA);
    }
    if (knockB.length) {
        knockB = translate(jmid, rotate(orient, translate([jlen/2, 0, 0], knockB)));
        objB = difference(objB, knockB);
    }

    return [ objA, objB ];
}





// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   PCB & PARTS
//

// pcboard
const pcbw = mils(5700);
const pcbd = mils(2100);
const pcbt = 1.6;               // thickness
const pcbHoleDrill = mils(100);
const pcbHoleInset = mils(100);

const pcbx0 = -pcbw/2;
const pcby0 = -pcbd/2;

// const pinh = inch(1.00);
// const pindia = inch(69/500);     // pins are 6-32 screws

const pinh = 16;
const pindia = 3;     // pins are M3 screws

const banodia1 = 12.7;
const banodia2 = 12.0;
const banidia = 4.22;
const banh = inch(0.25);

/*
// big knob
const knobAdia = inch(0.79);
const knobBdia = inch(0.66);
const knobCdia = inch(0.60);
const knobAh = inch(0.18);
const knobBh = inch(0.65);
const knobCh = inch(0.70);
*/
// small knob
const knobAdia = 15.9;
const knobBdia = 11.4;
const knobCdia = 10;
const knobAh = 4.1;
const knobBh = 13;
const knobCh = 13.8;

const knobdia = Math.max(knobAdia, knobBdia, knobCdia);
const knobh = Math.max(knobAh, knobBh, knobCh);

const knobshaftdia = 6;
const knobbushingdia = 7;   // it is M7 x 0.75 threaded
const knobx = pcbx0 + mils(4298.5);
const knoby = pcby0 + mils(424);

const platet = 1.5;         // thickness of acrylic plate

const standoffBoard = 12;   // standoff between box and main pcb
const standoffPlate = 8;    // standoff between main pcb and acrylic plate

const screwDia = 4.5;
const screwh = 2;

// the "feet" of the pcb assembly are just standoff screws
const footh = screwh;
const footHoleDia = screwDia + 1;
    // accomodates either the screw heads or standoffs,
    // this fits the nylon hardware, some metal hardware require 6

const pcbMountXYs = (function(){
    const x = pcbw / 2 - pcbHoleInset;
    const y = pcbd / 2 - pcbHoleInset;
    const hShort = t + standoffBoard + pcbt;
    const hLong = hShort + standoffPlate + platet;

    return [
        { x:  x, y:  y, h: hLong},
        { x:  x, y: -y, h: hLong},
        { x:  0, y:  y, h: hLong},
        { x:  0, y: -y, h: hLong},
        { x: -x, y:  y, h: hShort},
        { x: -x, y: -y, h: hShort}
        ];
    })();

const pinXYs = [
        { x: pcbx0 + mils(1050), y: pcby0 + mils(1050) },
        { x: pcbx0 + mils(1550), y: pcby0 + mils(1050) },
        { x: pcbx0 + mils(2050), y: pcby0 + mils(1050) },
        { x: pcbx0 + mils(2550), y: pcby0 + mils(1050) },
        { x: pcbx0 + mils( 250), y: pcby0 + mils( 800) },
        { x: pcbx0 + mils( 250), y: pcby0 + mils(1300) }
    ];

const redBananaXYs = [
        { x: pcbx0 + mils( 500), y: pcby0 + mils( 300) },
        { x: pcbx0 + mils(1100), y: pcby0 + mils( 300) },
        { x: pcbx0 + mils(1700), y: pcby0 + mils( 300) },
        { x: pcbx0 + mils(2300), y: pcby0 + mils( 300) },
        { x: pcbx0 + mils( 500), y: pcby0 + mils(1800) },
        { x: pcbx0 + mils(1100), y: pcby0 + mils(1800) }
    ];

const blackBananaXYs = [
        { x: pcbx0 + mils(2300), y: pcby0 + mils(1800) }
    ];

const bananaXYs = redBananaXYs.concat(blackBananaXYs);


const featherw = inch(2);
const featherd = inch(0.9);
const feathert = pcbt;

const oledx0 = pcbx0 + mils(3400);
const oledy0 = pcby0 + mils(950);

const usbx1 = pcbx0 + pcbw;
const usbyc = pcby0 + mils(1400);
const usbw = 7.86;   // centered on usbyc
const usbd = 5.05;   // from usbx1 back
const usbh = 2.70;   // from surface of pcb up

const powerx0 = pcbx0 + mils(5200);
const powery0 = pcby0 + mils(500);

function replicateAtXYs(xys, item) {
    return union(xys.map(xy => translate([ xy.x, xy.y, 0], item)));
}

function pcbHoles(dia) {
    const hole = cylinder({r: dia / 2, h: t, center: true});
    return replicateAtXYs(pcbMountXYs, hole);
}

function punchComponentHoles(obj) {
    const [omin, omax] = obj.getBounds();
    const oh = omax.z - omin.z;
    const gap = 0.5;

    return difference(obj,
        translate([0, 0, omin.z],
            union(
                replicateAtXYs(pinXYs,
                    cylinder({ r: pindia/2 + gap, h: oh, center: ttf})),
                // replicateAtXYs(bananaXYs,
                //     cylinder({ r: banodia1/2 + gap, h: oh, center: ttf})),
                translate([knobx, knoby, 0],
                    cylinder({ r: knobdia/2 + gap, h: oh, center: ttf}))
        )));
}


function clearPlate() {
    const dw = mils(100);
    const w = pcbw/2 + dw;
    const d = pcbd;
    const h = platet;
    const r = mils(100);

    return difference(
        colorize([0.98, 0.98, 1.0, 0.3],
            union(
                translate([-dw, 0, 0],          cube({size: [w, d - 2*r, h], center: ftt})),
                translate([-dw+r, 0, 0],            cube({size: [w - 2*r, d, h], center: ftt})),
                translate([-(dw-r), -(d/2-r), 0],   cylinder({r: r, h: h, center: true})),
                translate([-(dw-r), +(d/2-r), 0],   cylinder({r: r, h: h, center: true})),
                translate([(w-dw-r), -(d/2-r), 0],   cylinder({r: r, h: h, center: true})),
                translate([(w-dw-r), +(d/2-r), 0],   cylinder({r: r, h: h, center: true}))
            )
        ),
        pcbHoles(pcbHoleDrill),
        translate([knobx, knoby, 0],
            cylinder({ r: knobbushingdia/2 + 0.5, h: 3*h, center: true }))
    );
}

function boardAndPins() {
    const pcbColor =     hsv2rgb(120/360, 0.85, 0.48);
    const featherColor = hsv2rgb(120/360, 0.80, 0.30);


    const pcb =
        translate([0, 0, pcbt/2],
            colorize(pcbColor,
                difference(
                    cube({size: [pcbw, pcbd, pcbt], center: true}),
                    pcbHoles(pcbHoleDrill))));

    const pin =
        colorize(hsv2rgb(206/360, 0.03, 0.97),
            cylinder({r: pindia / 2, h: pinh, center: ttf}));


    const bananaJack =
        colorize(hsv2rgb(0/360, 0.8, 0.5),
            difference(
                cylinder({r1: banodia1/2, r2: banodia2/2, h: banh, center: ttf}),
                cylinder({r: banidia/2, h: banh, center: ttf})
                ));


    const featherUpperGap = 2.54;
    const featherOLED =
        union(
            colorize(featherColor,
                translate([0, 0, featherUpperGap],
                    cube({size: [featherw, featherd, feathert]}))),
            translate(
                [inch(0.28), inch(0.21), featherUpperGap + feathert],
                colorize(hsv2rgb(0, 0, 0),
                    cube({size: [inch(1.2), inch(0.45), 0.5]}))),
            // spacers on pins
            translate([mils(200), 0, 0],
                cube({size: [mils(1200), mils(100), featherUpperGap]})),
            translate([mils(200), mils(800), 0],
                cube({size: [mils(1600), mils(100), featherUpperGap]}))

        );

    const usb =
        colorize(hsv2rgb(206/360, 0.03, 0.97),
            cube({ size: [usbd, usbw, usbh], center: [false, true, false]})
        );

    const powerJack =
        union(
            colorize(hsv2rgb(0, 0, 0.15),
                difference(
                    cube({size:[ 13.5, 9, 11 ], center: [false, true, false]}),
                    translate([1, 0, 4.5],
                        rotate([0, 90, 0],
                            cylinder({r: 6/2, h: 12.5})))
                    )
                ),
            colorize(hsv2rgb(206/360, 0.03, 0.97),
                translate([0, 0, 4.5],
                    rotate([0, 90, 0],
                        cylinder({r: 1.95/2, h: 13.5})))
                )
        );

    const knob =
        union(
            colorize(hsv2rgb(206/360, 0.03, 0.97),
                union(
                    cube({ size: [12.5, 13.4, 6.5], center: ttf}),
                    cylinder({ r: 6/2, h: 6.5 + 15, center: ttf})
                )),
            translate([0, 0, 6.5 + 15 - 12.1],
                colorize(hsv2rgb(0, 0, 0.11),
                    cylinder({ r1: knobAdia/2, r2: knobAdia*0.95/2, h: knobAh, center: ttf})),
                colorize(hsv2rgb(0, 0, 0.11),
                    cylinder({ r1: knobBdia/2, r2: knobBdia*0.95/2, h: knobBh, center: ttf})),
                colorize(hsv2rgb(0, 0, 0.89),
                    cylinder({ r1: knobCdia/2, r2: knobCdia*0.95/2, h: knobCh, center: ttf}))
                )
            );

    const plate = translate([0, 0, 1.5/2], clearPlate());

    const screwHead =
        intersection(
            cylinder({r: screwDia/2, h: screwh}),
            translate([0, 0, -1.2*screwDia + screwh],
                sphere({r: 1.2*screwDia, center: true}))
            );

    const standoffs =
        union(pcbMountXYs.map(p =>
            translate([p.x, p.y, 0],
                colorize(hsv2rgb(0, 0, 0.31),
                    union(
                        translate([0, 0, p.h], screwHead),
                        rotate([0, 180, 0], screwHead),
                        cylinder({r: 4/Math.sqrt(3), fn: 6, h: p.h})
                    )))));

    return union(
        pcb,

        replicateAtXYs(pinXYs, pin),

        translate([0, 0, pcbt],
            [ replicateAtXYs(redBananaXYs, bananaJack),
              replicateAtXYs(blackBananaXYs,
                colorize(hsv2rgb(0, 0, 0.05), bananaJack))
            ]),

        translate([oledx0, oledy0, pcbt], featherOLED),
        translate([usbx1-usbd, usbyc, pcbt], usb),

        translate([powerx0, powery0, -11], powerJack),

        // the button
        translate([knobx, knoby, pcbt], knob),


        translate([0, 0, -(t + standoffBoard)], standoffs),

        translate([0, 0, pcbt + standoffPlate], plate)
        );
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   PULSAR-23
//

const jackSpacing = 25;
const jackDia = 18;
const jackGap = jackSpacing - jackDia;
const jackHeight = 15.5 + jackDia/2;

const jackClearW = jackDia/2 + 5*jackSpacing + jackDia/2;
const jackClearH = jackHeight + jackDia/2;

const rubberFootH = 7;

function pulsar23() {
    const body =
        translate([0, -280, rubberFootH],
            colorize([247/255, 247/255, 245/255, 0.7],
                cube({size:[380, 280, pulsarH - rubberFootH], center: false})));

    const nut =
        rotate([-90, 0, 0],
            colorize([0, 0, 0.25],
                union(
                    cylinder({r: jackDia/2, fn: 6, h: 4.5}),
                    cylinder({r: jackDia/2, h: 0.5}))));
    const nuts =
        union(
            translate([ 50, 0, 0], nut),
            translate([ 75, 0, 0], nut),
            translate([100, 0, 0], nut),
            translate([125, 0, 0], nut),
            translate([150, 0, 0], nut),
            translate([175, 0, 0], nut),
            translate([200, 0, 0], nut)
        )
    return union(
        body,
        translate([0, 0, jackHeight], nuts)
        );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   BOXES
//


const pcbNest = 3; // depth of PCB inside top box


const clearance = 20.0;                 // gap to cover rear jacks
const foot =  8;                   // foot size

const boxGap = 0.75;  // gap between inner and outer box
const cutGap = 1.0;         // gap between pieces when cut
const pcbGap = 0.5;   // gap between pcb nd box


// main dimensions
const ih = t + standoffBoard + pcbt + pcbNest;
const oh = pulsarH - 10;
const oTop = pulsarH - ih;

// const ow = 6*jackSpacing + 2*t/2;
// const iw = ow - 2*t - 2*boxGap;

const iw = pcbw + 2*pcbGap + 2*t;
const ow = iw + 2*boxGap + 2*t;

const id = pcbd + 2*pcbGap + 2*t; // 60.0;
const od = id + 2*boxGap + 2*t;

const nestingDepth = 11;    // height of inner box when nested

console.log("inner dimensions:", iw, "x", id, "x", ih);
console.log("outer dimensions:", ow, "x", od, "x", oh);


function outerBox(flat, clear) {
    const c = hsv2rgb(42/360, 0.46, 0.99).concat(clear ? [0.35] : []);

    const parts = boxParts(ow, od, oh, c);

    const tbTrim =
        punchComponentHoles(
            difference(parts.tb, pcbHoles(footHoleDia)));

    const fbTrim =
        difference(parts.fb,
            translate([0, (oh - jackClearH)/2 + 2, 0],
                cube({ size: [ow - 4*t, jackClearH, t], center: true }))
            ,
            translate([0, 0.7 - 5, 0],
                intersection(
                    scale([7.5, 1, 1], cylinder({ r: 10, h: t, fn: 80, center: true })),
                    cube({ size: [ow-4*t, oh, t], center: true})
                )
            )
            );

    const lrTrim =
        difference(parts.lr,
            translate([(oh-(t+nestingDepth))/2, 0, 0],
                cube({ size: [t+nestingDepth, od - 2*foot, t], center: true })),
            translate([-(oh-(ih - 10))/2, 0, 0],
                cube({ size: [ih - 10, od - 2*foot, t], center: true }))

            );

    var top =   translate([0, 0, oTop - t/2],                                tbTrim);
    var bottom =translate([0, 0, t/2],                                   tbTrim);
    var front = translate([0, -((od-t)/2), oh/2],   rotate([90, 180, 0],    fbTrim));
    var back =  translate([0, (od-t)/2, oh/2],      rotate([-90, 0, 0],     fbTrim));
    var left =  translate([-(ow-t)/2, 0, oh/2],     rotate([180, 90, 0],    lrTrim));
    var right = translate([(ow-t)/2, 0, oh/2],      rotate([0, 90, 0],      lrTrim));

    bottom = null;  // no bottom

    const topEdgeInset = 45;

    [front, top]    = tabJoin(front, top,   [0, 0, 0],      { indent: topEdgeInset });
    [back,  top]    = tabJoin(back,  top,   [0, 0, 0],      { indent: topEdgeInset });
    [left,  top]    = tabJoin(left,  top,   [0, 0, 90]);
    [right, top]    = tabJoin(right, top,   [0, 0, -90]);

    [bottom, front] = tabJoin(bottom, front,    [0, 0, 0],      { indent: longEdgeInset });
    [bottom, back]  = tabJoin(bottom, back,     [0, 0, 0],      { indent: longEdgeInset });
    [bottom, left]  = tabJoin(bottom, left,     [0, 0, 90]);
    [bottom, right] = tabJoin(bottom, right,    [0, 0, -90]);

    [front, left]   = tabJoin(front, left,  [0, 90, 0],     { tab: sideTab });
    [front, right]  = tabJoin(front, right, [0, -90, 0],    { tab: sideTab });
    [back, left]    = tabJoin(back, left,   [0, 90, 0],     { tab: sideTab });
    [back, right]   = tabJoin(back, right,  [0, -90, 0],    { tab: sideTab });

    if (flat) {

        // deconstruct the box
        top =                         translate([0, 0, -(oTop - t/2)],        top);
        front = rotate([90, -180, 0],  translate([0, (od-t)/2, -oh/2],   front));
        back =  rotate([90, 0, 0],   translate([0, -(od-t)/2, -oh/2],      back));
        left =  rotate([180, 90, 0],  translate([(ow-t)/2, 0, -oh/2],     left));
        right = rotate([0, -90, 0],    translate([-(ow-t)/2, 0, -oh/2],      right));

        // lay it out
        const [smin, smax] = fbTrim.getBounds();
        const sh = smax.y - smin.y;
        const bh = oh - oTop + t;

        top =   translate([0, -oh - od/2 - cutGap, 0],                  top);
        front = translate([0, -oh/2,        0],                         front);
        back =  translate([0,  oh/2+cutGap + 3, 0],     rotate([0, 0, 180], back));
        left =  translate([-(od+cutGap)/2, -8, 0],  rotate([0, 0, 90],  left));
        right = translate([(od+cutGap)/2,  -8, 0],  rotate([0, 0, 90],  right));
    }

    return union([top, bottom, left, right, front, back].filter(x => x));
}



function innerBox(flat) {
    const parts = boxParts(iw, id, ih, hsv2rgb(42/360, 0.66, 0.99));

    const topCover = punchComponentHoles(parts.tb);

    const botFoot =
        difference(
            unionInColor(
                parts.tb,
                face(ow, od - 2*foot - 2*boxGap))
            , pcbHoles(pcbHoleDrill));

    const lTrim =
        difference(parts.lr,
            intersection(
                translate([-t/2, 0, 0], cube({ size: [8, id-6*t, t], center: true })),
                union((function(){
                    const slat = rotate([0, 0, 45], cube({size: [2*iw, 1.5, t], center: true} ));

                    var r = [];
                    for (y = 0; y <= id; y += 3*sqrt(2)) {
                        r.push(translate([0, y, 0], slat));
                        if (y>0) {
                            r.push(translate([0, -y, 0], slat));
                        }
                    }

                    return r;
                })())
            ));

    const rTrim = (function(){

        const frame = function(w, h, x, y){
            const fw = t;

            return {
                hole: translate([x, y, 0],
                    cube({ size: [w, h, t], center: true })),
                edge: translate([x, y, 0],
                    cube({ size: [w + 2*fw, h + 2*fw, t], center: true }))
            };
        };

        const a = frame(standoffBoard, 11.8,
                (ih - standoffBoard)/2 - t, powery0);

        const b = frame(9.5, 12,
                ih/2 - standoffBoard - t - pcbt - usbh/2, usbyc);

        const full =
            union(lTrim,
                intersection(parts.lr, a.edge),
                intersection(parts.lr, b.edge)
            );
        return difference(full, a.hole, b.hole);
    })();

    var top =   translate([0, 0, ih - t/2],        rotate([180, 180, 180],   topCover));
    var bottom =translate([0, 0, t/2],            rotate([0, 180, 0],     botFoot));
    var front = translate([0, -((id-t)/2), ih/2],   rotate([90, 180, 0],    parts.fb));
    var back =  translate([0, (id-t)/2, ih/2],      rotate([-90, 0, 0],     parts.fb));
    var left =  translate([-(iw-t)/2, 0, ih/2],     rotate([180, 90, 0],    lTrim));
    var right = translate([(iw-t)/2, 0, ih/2],      rotate([0, 90, 0],      rTrim));

    top = null;

    [top, front] = tabJoin(top, front,    [0, 0, 0],      { indent: longEdgeInset });
    [top, back]  = tabJoin(top, back,     [0, 0, 0],      { indent: longEdgeInset });
    [top, left]  = tabJoin(top, left,     [0, 0, 90]);
    [top, right] = tabJoin(top, right,    [0, 0, -90]);

    [bottom, front] = tabJoin(bottom, front,    [0, 0, 0],      { indent: longEdgeInset });
    [bottom, back]  = tabJoin(bottom, back,     [0, 0, 0],      { indent: longEdgeInset });
    [bottom, left]  = tabJoin(bottom, left,     [0, 0, 90]);
    [bottom, right] = tabJoin(bottom, right,    [0, 0, -90],    { indent: 20 });

    [front, left]   = tabJoin(front, left,  [0, 90, 0],         { tab: sideTab });
    [front, right]  = tabJoin(front, right, [0, -90, 0],        { tab: sideTab });
    [back, left]    = tabJoin(back, left,   [0, 90, 0],         { tab: sideTab });
    [back, right]   = tabJoin(back, right,  [0, -90, 0],        { tab: sideTab });

    if (flat) {
        // deconstruct the box
        //top =                           translate([0, 0, -(ih - t/2)],   top);
        bottom =                         translate([0, 0, -t/2],        bottom);
        front = rotate([90, -180, 0],  translate([0, (id-t)/2, -ih/2],   front));
        back =  rotate([90, 0, 0],   translate([0, -(id-t)/2, -ih/2],      back));
        left =  rotate([180, 90, 0],  translate([(iw-t)/2, 0, -ih/2],     left));
        right = rotate([0, -90, 0],    translate([-(iw-t)/2, 0, -ih/2],      right));

        // lay it out
        const [emin, emax] = rTrim.getBounds();
        const eh = emax.x - emin.x;

        bottom =translate([0, 0, 0],                            rotate([0, 180, 0], bottom));
        front = translate([0, ((ih+id)/2 + cutGap), 0],          rotate([0, 0, 0], front));
        back =  translate([0, ((ih+id)/2 + ih + 2*cutGap), 0],    rotate([0, 0, 0], back));
        left =  translate([-(id+cutGap)/2, -(eh + id)/2 - cutGap - 16, 0], rotate([0, 0, 90], left));
        right = translate([(id+cutGap)/2,  -(eh + id)/2 - cutGap - 16, 0], rotate([0, 0, 90], right));
    }

    return union([top, bottom, left, right, front, back].filter(x => x));
}

function placedPcb() {
    return translate([0, 0, t + standoffBoard], boardAndPins());
}

function getParameterDefinitions() {
    const values = [ 'flat', 'nested', 'nested solid', 'stacked', 'pulsar',
                'apart', 'outer only', 'inner only', 'pcb only', 'svg', 'plate']
    return [
        { name: 'layout', type: 'choice', caption: 'layout',
            values: values,
            captions: values,
            default: 'flat' },
        { name: 'pcb', type: 'checkbox', caption: 'show pcb',
            checked: true }
    ];
}

function main(params) {

  if (params.layout == 'plate') {
    const plate = clearPlate();
    const [pmin, pmax] = plate.getBounds();
    console.log("Flat extent: " + (pmax.x - pmin.x) + "mm x " + (pmax.y - pmin.y) + "mm");
    return plate.projectToOrthoNormalBasis(CSG.OrthoNormalBasis.Z0Plane());
  }

  const flat = params.layout == 'flat' || params.layout == 'svg';
  var boxA = outerBox(flat, params.layout == 'nested');
  var boxB = innerBox(flat);

  if (flat) {
      const [amin, amax] = boxA.getBounds();
      const [bmin, bmax] = boxB.getBounds();

      const jigsaw = union(
        translate([0, -(amax.y + cutGap/2), 0], boxA),
        translate([0,  -(bmin.y - cutGap/2) - (id/2) - 6, 0], boxB)
        );

      const [jmin, jmax] = jigsaw.getBounds();
      console.log("Flat extent: " + (jmax.x - jmin.x) + "mm x " + (jmax.y - jmin.y) + "mm");

      if (params.layout == 'svg') {
        return jigsaw.projectToOrthoNormalBasis(CSG.OrthoNormalBasis.Z0Plane());
      }
      return jigsaw;
  }

  if (params.pcb) {
    boxB = union(boxB, placedPcb());
  }

  if (params.layout == 'nested' || params.layout == 'nested solid')
      return union(translate([0, 0, nestingDepth], boxB), translate([0, 0, 0], boxA));
  if (params.layout == 'stacked')
      return union(translate([0, 0, 0], boxA), translate([0, 0, oTop], boxB));
  if (params.layout == 'pulsar')
      return union(translate([0, 0, 0], boxA), translate([0, 0, oTop], boxB),
        translate([-ow/2-59.5, -od/2, 0], pulsar23()));
  if (params.layout == 'apart')
      return union(translate([0, od, 0], boxA), translate([0, -od, footh], boxB));
  if (params.layout == 'outer only')
      return boxA;
  if (params.layout == 'inner only')
      return translate([0, 0, footh], boxB);
  if (params.layout == 'pcb only')
      return boardAndPins();
  return [];
}

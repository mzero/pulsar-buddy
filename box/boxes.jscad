
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   UTILITIES
//

const ttf = [true, true, false];

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

const pulsarH = 80.0;



// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   BASIC BOXES
//

const defaultTab = 3*t;
const defaultFit = 0;       // k?
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

const pinh = inch(1.00);
const pindia = inch(0.138);     // pins are 6-32 screws

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
const knobx = pcbx0 + mils(4300 + 200);
const knoby = pcby0 + mils(400);



const pcbMountXYs = (function(){
    const x = pcbw / 2 - pcbHoleInset;
    const y = pcbd / 2 - pcbHoleInset;

    return [
        { x:  x, y:  y},
        { x:  x, y: -y},
        { x:  0, y:  y},
        { x:  0, y: -y},
        { x: -x, y:  y},
        { x: -x, y: -y}
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

const featherhBelow = pcbt + 8 + 1;  // 1 for solder
const featherhAbove = 2.54 + pcbt;
const featherh = featherhBelow + pcbt + featherhAbove;

const featherx0 = pcbx0 + mils(3700);
const feathery0 = pcby0 + mils(850);



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
                replicateAtXYs(bananaXYs,
                    cylinder({ r: banodia1/2 + gap, h: oh, center: ttf})),
                translate([knobx, knoby, 0],
                    cylinder({ r: knobshaftdia/2 + gap, h: oh, center: ttf})),
                translate([featherx0 - gap, feathery0 - gap, 0],
                    cube({ size: [featherw + 2*gap, featherd + 2*gap, oh], center: false}))
        )));
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

    const feather =
        union(
            colorize(featherColor,
                cube({size: [featherw, featherd, featherh]})),
            translate(
                [inch(0.28), inch(0.21), featherh],
                colorize(hsv2rgb(0, 0, 0),
                    cube({size: [inch(1.2), inch(0.45), 0.5]})))
            // translate(
            //     [0, 0, featherh + 4],
            //     colorize([0.98, 0.98, 1.0, 0.3],
            //         cube({size: [featherw, featherd, 1.5]})))
        );

    const featherOLED =
        union(
            colorize(featherColor,
                translate([0, 0, 2.54],
                    cube({size: [featherw, featherd, feathert]}))),
            translate(
                [inch(0.28), inch(0.21), 2.54 + feathert],
                colorize(hsv2rgb(0, 0, 0),
                    cube({size: [inch(1.2), inch(0.45), 0.5]}))),
            // spacers on pins
            translate([mils(200), 0, 0],
                cube({size: [mils(1200), mils(100), 2.54]})),
            translate([mils(200), mils(800), 0],
                cube({size: [mils(1600), mils(100), 2.54]}))

        );

    const featherLowerGap = 8.5 + 2.54;
    const featherM0 =
        union(
            // socket & spacers on pins
            translate([mils(200), 0, -featherLowerGap],
                cube({size: [mils(1200), mils(100), featherLowerGap]})),
            translate([mils(200), mils(800), -featherLowerGap],
                cube({size: [mils(1600), mils(100), featherLowerGap]})),

            colorize(featherColor,
                translate([0, 0, -(featherLowerGap + feathert)],
                    cube({size: [featherw, featherd, feathert]})))
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

    const plate =
        translate([0, 0, 1.5/2],
            difference(
                translate([-mils(100), 0, 0],
                    colorize([0.98, 0.98, 1.0, 0.3],
                            cube({size: [pcbw/2 + mils(100), pcbd, 1.5], center: [false, true, true]}))),
                pcbHoles(pcbHoleDrill),
                translate([knobx, knoby, 0],
                    cylinder({ r: knobshaftdia/2 + 0.5, h: oh, center: ttf}))
                ));

    return union(
        pcb,

        replicateAtXYs(pinXYs, pin),

        translate([0, 0, pcbt],
            [ replicateAtXYs(redBananaXYs, bananaJack),
              replicateAtXYs(blackBananaXYs,
                colorize(hsv2rgb(0, 0, 0.05), bananaJack))
            ]),

        // the feather stack
        //translate([featherx0, feathery0, -featherhBelow], feather),
        translate([featherx0, feathery0 + mils(100), pcbt], featherOLED),
        translate([featherx0, feathery0, 0], featherM0),

        translate([pcbx0 + mils(5200), pcby0 + mils(600), -11], powerJack),

        // the button
        translate([knobx, knoby, pcbt], knob),

        translate([0, 0, pcbt + 8], plate)
        );
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   PULSAR-23
//

function pulsar23() {
    const body =
        translate([0, -280, 0],
            colorize([247/255, 247/255, 245/255, 0.7],
                cube({size:[380, 280, 80], center: false})));

    const nut =
        rotate([-90, 0, 0],
            colorize([0, 0, 0.25],
                union(
                    cylinder({r: 17.2/2, fn: 6, h: 4.5}),
                    cylinder({r: 17.2/2, h: 0.5}))));
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
        translate([0, 0, 12.5], nuts)
        );
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//   BOXES
//


const standoffHeadDia = 5.5;    // to accomodate hex heads,
        // this fits the nylon ones, some metal ones require 6
const standoff1H = 15;
    // height of pcb off of bottom surface
    // has to be at least 9mm to accomodate the banana jacks
    // has to be at least 11.6mm to accomodate feather m0

const jackSpacing = 25;
const jackDia = 17.2;
const jackClear = jackDia/2 + 5*jackSpacing + jackDia/2;

// outer dimensions
const ow = 156; // ≥ jackClear + 2 * t;
const od = 60.0;
const oh = 51.2;

const clearance = 25.0;                 // gap to cover rear jacks
const foot = 6.0;                      // foot size

const boxGap = inch(0.01);  // gap between inner and outer box
const cutGap = 1.0;         // gap between pieces when cut



function outerBox(flat, clear) {
    const w = ow;
    const d = od + 2*t + 2*boxGap;
    const h = oh;
    const c = hsv2rgb(42/360, 0.46, 0.99).concat(clear ? [0.35] : []);

    const parts = boxParts(w, d, h, c);

    const tbTrim = difference(parts.tb, pcbHoles(standoffHeadDia));

    const fbTrim =
        union(
            difference(parts.fb,
                translate([0, (h - clearance)/2, 0],
                    cube({ size: [w, clearance, t], center: true })),
                scale([8, 1, 1],
                    translate([0, 5, 0],
                        cylinder({ r: 10, h: h, center: true })))
                ),
            translate([-w/2 + 3, 0, 0], cube({size: [6, h, t], center: true})),
            translate([w/2 - 3, 0, 0], cube({size: [6, h, t], center: true}))
            );

    const lrTrim =
        difference(parts.lr,
            translate([(h-t)/2, 0, 0],
                cube({ size: [t, od - 2*foot, t], center: true })));

    var top =   translate([0, 0, h - t/2],                                tbTrim);
    var bottom =translate([0, 0, t/2],                                   tbTrim);
    var front = translate([0, -((d-t)/2), h/2],   rotate([90, 180, 0],    fbTrim));
    var back =  translate([0, (d-t)/2, h/2],      rotate([-90, 0, 0],     fbTrim));
    var left =  translate([-(w-t)/2, 0, h/2],     rotate([180, 90, 0],    lrTrim));
    var right = translate([(w-t)/2, 0, h/2],      rotate([0, 90, 0],      lrTrim));

    bottom = null;  // no bottom

    [top, front]    = tabJoin(top, front,   [0, 0, 0],      { indent: longEdgeInset });
    [top, back]     = tabJoin(top, back,    [0, 0, 0],      { indent: longEdgeInset });
    [top, left]     = tabJoin(top, left,    [0, 0, 90]);
    [top, right]    = tabJoin(top, right,   [0, 0, -90]);

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
        top =                         translate([0, 0, -(h - t/2)],        top);
        front = rotate([90, -180, 0],  translate([0, (d-t)/2, -h/2],   front));
        back =  rotate([90, 0, 0],   translate([0, -(d-t)/2, -h/2],      back));
        left =  rotate([180, 90, 0],  translate([(w-t)/2, 0, -h/2],     left));
        right = rotate([0, -90, 0],    translate([-(w-t)/2, 0, -h/2],      right));

        // lay it out
        const [smin, smax] = fbTrim.getBounds();
        const sh = smax.y - smin.y;

        top =   translate([0, 0, 0], top);
        front = translate([0, -(smax.y + d/2 + cutGap), 0],           front);
        back =  translate([0, -(smax.y + d/2 + sh + 2*cutGap), 0],    back);
        left =  translate([-(d+cutGap)/2, (d+h)/2+cutGap, 0],         rotate([0, 0, 90], left));
        right = translate([(d+cutGap)/2, (d+h)/2+cutGap, 0],          rotate([0, 0, 90], right));
    }

    return union([top, bottom, left, right, front, back].filter(x => x));
}



function innerBox(flat) {
    const w = ow - 2*t - 2*boxGap;
    const d = od;
    const h = pulsarH - oh;

    const parts = boxParts(w, d, h, hsv2rgb(42/360, 0.46, 0.99));

    const topCover = punchComponentHoles(parts.tb);

    const botFoot =
        difference(
            unionInColor(
                parts.tb,
                face(ow, od - 2*foot - 2*boxGap))
            , pcbHoles(pcbHoleDrill));

    const lrTrim =
        difference(parts.lr,
            translate([(11-6)/2, 0, 0],
                cube({ size: [h - 11 - 6, d - 15, t], center: true })));

    var top =   translate([0, 0, h - t/2],        rotate([180, 180, 180],   topCover));
    var bottom =translate([0, 0, t/2],            rotate([0, 180, 0],     botFoot));
    var front = translate([0, -((d-t)/2), h/2],   rotate([90, 180, 0],    parts.fb));
    var back =  translate([0, (d-t)/2, h/2],      rotate([-90, 0, 0],     parts.fb));
    var left =  translate([-(w-t)/2, 0, h/2],     rotate([180, 90, 0],    parts.lr));
    var right = translate([(w-t)/2, 0, h/2],      rotate([0, 90, 0],      lrTrim));

    top = null;

    [top, front] = tabJoin(top, front,    [0, 0, 0],      { indent: longEdgeInset });
    [top, back]  = tabJoin(top, back,     [0, 0, 0],      { indent: longEdgeInset });
    [top, left]  = tabJoin(top, left,     [0, 0, 90]);
    [top, right] = tabJoin(top, right,    [0, 0, -90]);

    [bottom, front] = tabJoin(bottom, front,    [0, 0, 0],      { indent: longEdgeInset });
    [bottom, back]  = tabJoin(bottom, back,     [0, 0, 0],      { indent: longEdgeInset });
    [bottom, left]  = tabJoin(bottom, left,     [0, 0, 90]);
    [bottom, right] = tabJoin(bottom, right,    [0, 0, -90]);

    [front, left]   = tabJoin(front, left,  [0, 90, 0],         { tab: sideTab });
    [front, right]  = tabJoin(front, right, [0, -90, 0],        { tab: sideTab });
    [back, left]    = tabJoin(back, left,   [0, 90, 0],         { tab: sideTab });
    [back, right]   = tabJoin(back, right,  [0, -90, 0],        { tab: sideTab });

    if (flat) {
        // deconstruct the box
        //top =                           translate([0, 0, -(h - t/2)],   top);
        bottom =                         translate([0, 0, -t/2],        bottom);
        front = rotate([90, -180, 0],  translate([0, (d-t)/2, -h/2],   front));
        back =  rotate([90, 0, 0],   translate([0, -(d-t)/2, -h/2],      back));
        left =  rotate([180, 90, 0],  translate([(w-t)/2, 0, -h/2],     left));
        right = rotate([0, -90, 0],    translate([-(w-t)/2, 0, -h/2],      right));

        // lay it out
        const [emin, emax] = lrTrim.getBounds();
        const eh = emax.x - emin.x;

        bottom =translate([0, 0, 0],                            rotate([0, 180, 0], bottom));
        front = translate([0, -((h+d)/2 + cutGap), 0],          rotate([0, 0, 0], front));
        back =  translate([0, -((h+d)/2 + h + 2*cutGap), 0],    rotate([0, 0, 0], back));
        left =  translate([-(d+cutGap)/2, (eh + d)/2 + cutGap, 0], rotate([0, 0, 90], left));
        right = translate([(d+cutGap)/2,  (eh + d)/2 + cutGap, 0], rotate([0, 0, 90], right));
    }

    return union([top, bottom, left, right, front, back].filter(x => x));
}

function placedPcb() {
    return translate([0, 0, t + standoff1H], boardAndPins());
}

function getParameterDefinitions() {
    const values = [ 'flat', 'nested', 'nested solid', 'stacked', 'pulsar',
                'apart', 'outer only', 'inner only', 'pcb only', 'svg']
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
  const flat = params.layout == 'flat' || params.layout == 'svg';
  var boxA = outerBox(flat, params.layout == 'nested');
  var boxB = innerBox(flat);

  if (flat) {
      const [amin, amax] = boxA.getBounds();
      const [bmin, bmax] = boxB.getBounds();

      const jigsaw = union(
        translate([0, -(amax.y + cutGap/2), 0], boxA),
        translate([0,  -(bmin.y - cutGap/2), 0], boxB)
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
      return union(translate([0, 0, 0], boxB), translate([0, 0, 0], boxA));
  if (params.layout == 'stacked')
      return union(translate([0, 0, 0], boxA), translate([0, 0, oh], boxB));
  if (params.layout == 'pulsar')
      return union(translate([0, 0, 0], boxA), translate([0, 0, oh], boxB),
        translate([-ow/2-59.5, -od/2, 0], pulsar23()));
  if (params.layout == 'apart')
      return union(translate([0, od, 0], boxA), translate([0, -od, 0], boxB));
  if (params.layout == 'outer only')
      return boxA;
  if (params.layout == 'inner only')
      return boxB;
  if (params.layout == 'pcb only')
      return boardAndPins();
  return [];
}

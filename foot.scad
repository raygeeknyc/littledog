cube([40,3,10]);
translate([38,1,6]) {
        difference() {
            rotate([0,100,0]) {
                cylinder(r=7,h=3);
            }
            translate([-5,-5,-8]) {
                cube([15,15,2]);
            }
         }
}
translate([0,97,0]) {
	cube([40,3,10]);
	translate([38,1,6]) {
        difference() {
            rotate([0,100,0]) {
                cylinder(r=7,h=3);
            }
            translate([-5,-5,-8]) {
                cube([15,15,2]);
            }
         }
	}
}

difference() {
    union() {
        translate([0,40,10]) {
            cube([3,20,5]);
        }
        cube([3,100,10]);        
    }
	translate([0,39,7]) {
		rotate([0,90,0]) {
			translate([0,11,-1]) {
				cylinder(r=2,h=5);
			}
			translate([0,1.5,-1]) {
				cylinder(r=0.5,h=5);
			}
			translate([0,20.5,-1]) {
				cylinder(r=0.5,h=5);
			}
			translate([5.2,11,-1]) {
				cylinder(r=0.5,h=5);
			}
			translate([-5.2,11,-1]) {
				cylinder(r=0.5,h=5);
			}
		}
	}
}

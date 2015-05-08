difference() {
        cube ([50,52,4]);
			translate([13,13,0]) {
	      	cylinder (h = 8, r=8.5, center = true, $fn=100);
				translate([0,26,0]) {
	      		cylinder (h = 8, r=8.5, center = true, $fn=100);
				}
				translate([-10.5,7.5,0]) {
					cube([5,11,4]);
					translate([18,0.5,2]) {
						cube([3,10,2]);
					}
				}
			}
}
translate([47,10,0]) {
    difference() {
        rotate([0,10,0]){
            cube([3.25,32,10]);
        }
        translate([0,0,-4]) {
            cube([5,32,4]);
        }
    }
}

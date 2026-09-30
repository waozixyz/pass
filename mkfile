</$objtype/mkfile

# Generate with `make plan9-c` on the host, then build natively with `mk`.
GUI_FILES=`{ls build/plan9/gui/*.c}
CLI_FILES=`{ls build/plan9/cli/*.c}
GUI_HEADERS=`{ls build/plan9/gui/*.h}
CLI_HEADERS=`{ls build/plan9/cli/*.h}
GUI_OBJECTS=${GUI_FILES:%.c=%.$O}
CLI_OBJECTS=${CLI_FILES:%.c=%.$O}
CFLAGS=-FTVw
LIB=/$objtype/lib/libdraw.a /$objtype/lib/libthread.a

all:V: pass-gui pass

pass-gui: $GUI_OBJECTS $LIB
	$LD -o $target $prereq

pass: $CLI_OBJECTS
	$LD -o $target $prereq

$GUI_OBJECTS: $GUI_HEADERS
$CLI_OBJECTS: $CLI_HEADERS

%.$O: %.c
	$CC $CFLAGS -o $target $stem.c

install:V: pass-gui pass
	cp pass-gui /$objtype/bin/pass-gui
	cp pass /$objtype/bin/pass
	mkdir -p /sys/lib/pass
	cp build/plan9/assets/fingerprint.bit /sys/lib/pass/fingerprint.bit

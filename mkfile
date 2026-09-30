< /$objtype/mkfile

# The Ziran core is generated on the host by make plan9-c. Native GUI input
# awaits Kryon's native libdraw provider; do not compile the removed C app.
TARG=pass_core.a
OFILES=build/plan9/core/pass_core.$O
CFLAGS=-Ibuild/plan9/core

$TARG: $OFILES
	ar vu $target $prereq

build/plan9/core/%.$O: build/plan9/core/%.c
	$CC $CFLAGS -o $target $prereq

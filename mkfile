< /$objtype/mkfile

# Native Plan 9 build of Pass (Kryon libdraw backend).
#
# build/plan9 must be prepared on the host before building here:
#
#	make kry-c && python3 scripts/prepare-plan9-generated-c.py
#
# which rewrites the k2c output for 8c and generates
# build/plan9/{generated, generated-c-files.txt, pass_embedded_assets.c}.
# This mkfile compiles those together with the runtime, the crypto core,
# and the native entry point, and links against /$objtype/lib/libkryon.a.

TARG=pass
ROOT=/sys/src/pass
KRYON=/sys/src/kryon
BIN=/$objtype/bin
OUT=$O.out

obj=$ROOT/build/plan9/obj
list=$ROOT/build/plan9/generated-c-files.txt

CPPFLAGS=-I$KRYON/src/platform/plan9/include -I$KRYON/include -I$KRYON/src -I$KRYON/src/ui \
	-I$ROOT/build/plan9/generated -I$ROOT/native \
	-DANDROID_BUILD=0 -DPLATFORM_DESKTOP=1 -DKRYON_BACKEND_LIBDRAW=1 -DKRYON_PLATFORM_PLAN9=1 \
	-DKRYON_NATIVE_PLAN9=1 -DUI_EMBEDDED_ONLY=1

CFLAGS=-FTVw

gensrc=`{cat $list}
appsrc=native/pass_core.c native/pass_runtime.c native/pass_plan9_main.c
hostsrc=build/plan9/pass_embedded_assets.c

allsrc=`{echo $gensrc $appsrc $hostsrc | tr ' ' '\12' | grep -v '^$'}
OFILES=`{echo $allsrc | tr ' ' '\12' | sed -e 's@\.c$@.8@' -e 's@^@'$obj'/@'}

check:V:
	if(! test -f $list) {
		echo 'missing '^$list^'; run scripts/prepare-plan9-generated-c.py on the host first' >[1=2]
		exit missing
	}

all:V: check $OUT

install:V: check $BIN/$TARG

$BIN/$TARG: $OUT
	cp $OUT $BIN/$TARG

$OUT: $OFILES /$objtype/lib/libkryon.a
	$LD -o $target $prereq -lkryon -ldraw -lmemdraw -lthread

$obj/%.8: %.c
	mkdir -p `{echo $target | sed 's@/[^/]*$@@'} && cpp -+ $CPPFLAGS $prereq > $obj/$stem.i && $CC $CFLAGS -o $target -c $obj/$stem.i && rm -f $obj/$stem.i

clean:V:
	rm -rf $obj [$OS].out

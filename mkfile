< /$objtype/mkfile

# Native Plan 9 build of Pass (Kryon libdraw backend). build/plan9 is
# prepared on the host by: make kry-c-plan9

TARG=pass
ROOT=/sys/src/pass

gensrc=`{cat $ROOT/build/plan9/generated-c-files.txt}
appsrc=native/pass_core.c native/pass_runtime.c native/pass_plan9_main.c
hostsrc=build/plan9/pass_embedded_assets.c
APPCPPFLAGS=-I$ROOT/build/plan9/generated -I$ROOT/native
LDLIBS=

< /sys/src/kryon/mk/plan9-app.mk

#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
kryon_dir=${KRYON_DIR:-"$root/vendor/kryon"}
arch=$(uname -m)
if [ "$arch" = "amd64" ]; then
    arch=x86_64
fi
platform=$(uname -s | tr '[:upper:]' '[:lower:]')
build_dir="$kryon_dir/build/$platform-$arch"
k2g="$build_dir/bin/k2g"
k2c="$build_dir/bin/k2c"
work=${TMPDIR:-/tmp}/pass-kry-smoke.$$

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

make -C "$kryon_dir" k2g k2c

mkdir -p "$work/go" "$work/c"
"$k2g" --root "$root" -o "$work/go" "$root/app/pass.kry"
"$k2c" --root "$root" -o "$work/c" "$root/app/pass.kry"

sh "$kryon_dir/tests/check_clean_generated_output.sh" "$work/go"
sh "$kryon_dir/tests/check_clean_generated_output.sh" "$work/c"

cat > "$work/go/go.mod" <<EOF
module passkrysmoke

go 1.25.0

require (
	github.com/waozixyz/kryon/go/kryon v0.0.0
	github.com/waozixyz/pass v0.0.0
	golang.org/x/image v0.45.0 // indirect
	golang.org/x/sys v0.47.0 // indirect
	golang.org/x/text v0.41.0 // indirect
)

replace github.com/waozixyz/kryon/go/kryon => $kryon_dir/go/kryon
replace github.com/waozixyz/pass => $root
EOF
cp "$kryon_dir/go/kryon/go.sum" "$work/go/go.sum"
if [ -f "$root/go.sum" ]; then
    cat "$root/go.sum" >> "$work/go/go.sum"
    sort -u "$work/go/go.sum" -o "$work/go/go.sum"
fi

cat > "$work/go/pass_host_test.go" <<'EOF'
package krygen

import (
	"testing"

	kryon "github.com/waozixyz/kryon/go/kryon"
	password "github.com/waozixyz/pass"
)

type smokeHost struct {
	generated string
	status    string
	copied    string
	calls     int
	lastErr   string
	theme     [4]int32
}

var _ PassHost = (*smokeHost)(nil)

func (h *smokeHost) Generate(site string, login string, master string, length int32, counter int32, lower int32, upper int32, digits int32, symbols int32, exclude string) int32 {
	got, err := password.Generate(site, login, master, password.Options{
		Length:    int(length),
		Counter:   uint64(counter),
		Lowercase: lower != 0,
		Uppercase: upper != 0,
		Digits:    digits != 0,
		Symbols:   symbols != 0,
		Exclude:   exclude,
	})
	h.calls++
	if err != nil {
		h.generated = ""
		h.status = err.Error()
		h.lastErr = err.Error()
		return 1
	}
	h.generated = got
	h.status = "Password generated locally"
	return 0
}

func (h *smokeHost) Generated() string { return h.generated }
func (h *smokeHost) Status() string    { return h.status }
func (h *smokeHost) Copy() int32 {
	h.copied = h.generated
	return 0
}
func (h *smokeHost) SaveMaster(master string, requireBiometric int32) int32 { return 0 }
func (h *smokeHost) UnlockMaster() int32                                    { return 0 }
func (h *smokeHost) ClearMaster() int32                                     { return 0 }
func (h *smokeHost) ProfileCount() int32                                    { return 0 }
func (h *smokeHost) ProfileLabel(index int32) string                        { return "" }
func (h *smokeHost) ApplyProfile(index int32) int32                         { return 0 }
func (h *smokeHost) SaveProfile(name string, site string, login string, length int32, counter int32, lower int32, upper int32, digits int32, symbols int32, exclude string) int32 {
	return 0
}
func (h *smokeHost) DeleteProfile(index int32) int32 { return 0 }
func (h *smokeHost) SaveSettings(autoCopy int32, clearSeconds int32, showFingerprint int32, themeSource int32, themeMode int32, themeID int32, themeStyle int32) int32 {
	h.theme = [4]int32{themeSource, themeMode, themeID, themeStyle}
	return 0
}

func putCString(dst []byte, value string) {
	for i := range dst {
		dst[i] = 0
	}
	copy(dst, value)
}

func findButton(id int32) (kryon.FrameOp, bool) {
	for _, op := range kryon.FrameOps() {
		if op.Kind == kryon.FrameOpButton && op.ID == id {
			return op, true
		}
	}
	return kryon.FrameOp{}, false
}

func frame(st *PassState) {
	kryon.BeginFrame()
	Pass_PassFrame(st)
	kryon.EndFrame()
}

func TestPassKryGenerateCallsRealPasswordLibrary(t *testing.T) {
	rt := kryon.New(kryon.AppConfig{Width: 390, Height: 740})
	kryon.SetRuntime(rt)
	defer kryon.SetRuntime(nil)

	host := &smokeHost{status: "Ready"}
	SetPassHost(host)

	st := *PassStateValue
	putCString(st.Site[:], "example.com")
	putCString(st.Login[:], "alice")
	putCString(st.Master[:], "correct horse battery staple")
	st.Length = 16
	st.Counter = 1
	st.Lower = 1
	st.Upper = 1
	st.Digits = 1
	st.Symbols = 1

	frame(&st)
	generate, ok := findButton(40)
	if !ok {
		t.Fatal("generated frame did not expose Generate button id 40")
	}

	kryon.QueueTap(generate.Bounds.X+generate.Bounds.Width/2, generate.Bounds.Y+generate.Bounds.Height/2)
	frame(&st)

	if host.calls != 1 {
		t.Fatalf("Generate calls = %d, want 1", host.calls)
	}
	if host.lastErr != "" {
		t.Fatalf("Generate returned error: %s", host.lastErr)
	}

	want, err := password.Generate("example.com", "alice", "correct horse battery staple", password.Options{
		Length:    16,
		Counter:   1,
		Lowercase: true,
		Uppercase: true,
		Digits:    true,
		Symbols:   true,
	})
	if err != nil {
		t.Fatalf("password.Generate returned error for expected inputs: %v", err)
	}
	if host.generated != want {
		t.Fatalf("generated password = %q, want %q", host.generated, want)
	}

	frame(&st)
	for _, op := range kryon.FrameOps() {
		if op.Kind == kryon.FrameOpText && op.Text == want {
			return
		}
	}
	t.Fatalf("generated password %q was not rendered by the generated frame", want)
}
EOF

(cd "$work/go" && GOCACHE="${GOCACHE:-$work/go-cache}" go test ./...)
cc -fsyntax-only -I"$kryon_dir/include" -I"$work/c" "$work/c/app/pass.c"

echo "pass .kry smoke ok"

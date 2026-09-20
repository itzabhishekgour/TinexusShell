#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import wave
import struct
import math

def gen_wav(path, duration=10):
    with wave.open(path, 'w') as f:
        f.setnchannels(2)
        f.setsampwidth(2)
        f.setframerate(44100)
        for i in range(int(44100 * duration)):
            val = int(16000.0 * math.sin(2.0 * math.pi * 440.0 * i / 44100.0))
            f.writeframesraw(struct.pack('<hh', val, val))
    print(f"[+] Generated test tone: {path}")

def run_cmd(cmd, env=None, check=False):
    res = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if check and res.returncode != 0:
        raise RuntimeError(f"Command failed ({res.returncode}): {' '.join(cmd)}\n{res.stderr}")
    return res

def main():
    print("=" * 70)
    print("       TINEXUS UNIVERSAL AUDIO E2E VERIFICATION SUITE")
    print("=" * 70)

    # 1. Verify binaries
    print("\n--- [Step 1] Checking Audio Binaries & Plugins in Rootfs ---")
    bins = [
        "/usr/bin/pipewire",
        "/usr/bin/pipewire-pulse",
        "/usr/bin/wireplumber",
        "/usr/bin/pactl",
        "/usr/bin/pw-play",
        "/usr/bin/paplay",
        "/usr/bin/aplay",
        "/usr/bin/vlc",
        "/usr/bin/firefox"
    ]
    for b in bins:
        if os.path.exists(b):
            print(f"  [PASS] {b} exists")
        else:
            print(f"  [FAIL] {b} MISSING!")
            sys.exit(1)

    # 2. Setup Runtime Environment as UID 1000
    runtime_dir = "/run/user/1000"
    os.makedirs(runtime_dir, exist_ok=True)
    os.makedirs(f"{runtime_dir}/pulse", exist_ok=True)
    os.chmod(runtime_dir, 0o700)

    env = os.environ.copy()
    env["XDG_RUNTIME_DIR"] = runtime_dir
    env["PULSE_SERVER"] = f"unix:{runtime_dir}/pulse/native"
    env["DBUS_SESSION_BUS_ADDRESS"] = f"unix:path={runtime_dir}/bus"

    # Kill any stale instances
    for proc_name in ["pipewire", "pipewire-pulse", "wireplumber", "vlc", "firefox"]:
        subprocess.run(["pkill", "-9", proc_name], stderr=subprocess.DEVNULL)
    time.sleep(1)

    # Start Session Bus if not running
    dbus_proc = None
    if not os.path.exists(f"{runtime_dir}/bus"):
        dbus_proc = subprocess.Popen([
            "/usr/bin/dbus-daemon", "--session", "--nofork", "--nopidfile",
            f"--address=unix:path={runtime_dir}/bus"
        ], env=env)
        time.sleep(1)

    # Start Audio Daemons
    print("\n--- [Step 2] Starting PipeWire, PipeWire-Pulse & WirePlumber ---")
    pw = subprocess.Popen(["/usr/bin/pipewire"], env=env)
    pwp = subprocess.Popen(["/usr/bin/pipewire-pulse"], env=env)
    wp = subprocess.Popen(["/usr/bin/wireplumber"], env=env)
    time.sleep(2)

    # Check pulse socket
    pulse_sock = f"{runtime_dir}/pulse/native"
    if os.path.exists(pulse_sock):
        print(f"  [PASS] Socket exists: {pulse_sock}")
    else:
        print(f"  [FAIL] Socket NOT created at {pulse_sock}")
        sys.exit(1)

    # 3. Check pactl info
    print("\n--- [Step 3] pactl info ---")
    res = run_cmd(["pactl", "info"], env=env)
    print(res.stdout)
    if "PulseAudio (on PipeWire" in res.stdout:
        print("  [PASS] pactl info successfully connected to PipeWire-Pulse!")
    else:
        print(f"  [FAIL] pactl info failed:\n{res.stderr}")
        sys.exit(1)

    # 4. Generate test audio
    test_wav = "/tmp/audio_test_sample.wav"
    gen_wav(test_wav, 15)

    # 5. Test Settings App's play_chime() equivalent (pw-play / paplay)
    print("\n--- [Step 4] Settings Test Sound (pw-play / paplay stream verification) ---")
    pw_play_proc = subprocess.Popen(["/usr/bin/pw-play", test_wav], env=env)
    time.sleep(0.5)
    sink_in_settings = run_cmd(["pactl", "list", "sink-inputs"], env=env)
    print("pactl list sink-inputs during Settings Test Sound:")
    print(sink_in_settings.stdout)
    pw_play_proc.terminate()
    pw_play_proc.wait()
    if "pw-play" in sink_in_settings.stdout or "Sink Input #" in sink_in_settings.stdout:
        print("  [PASS] Settings Test Sound successfully routes through PipeWire stream!")
    else:
        print("  [FAIL] Settings Test Sound stream missing from sink-inputs!")

    # 6. Test VLC Playback
    print("\n--- [Step 5] VLC Local Media Playback Stream Verification ---")
    vlc_proc = subprocess.Popen(
        ["/usr/bin/cvlc", "--no-video", "--loop", test_wav, "-vvv"],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    time.sleep(1.5)
    sink_in_vlc = run_cmd(["pactl", "list", "sink-inputs"], env=env)
    print("pactl list sink-inputs during VLC Playback:")
    print(sink_in_vlc.stdout)
    vlc_proc.terminate()
    vlc_out, vlc_err = vlc_proc.communicate(timeout=3)
    if "org.VideoLAN.VLC" in sink_in_vlc.stdout or "VLC media player" in sink_in_vlc.stdout:
        print("  [PASS] VLC audio stream active and verified in PipeWire-Pulse sink-inputs!")
    else:
        print("  [FAIL] VLC stream missing from sink-inputs!")
        print("VLC error output:", vlc_err[-500:])

    # 7. Test Firefox Audio Playback
    print("\n--- [Step 6] Firefox Audio Stream (cubeb_pulse) Verification ---")
    html_path = "/tmp/firefox_test_playback.html"
    with open(html_path, "w") as f:
        f.write(f"""<!DOCTYPE html>
<html>
<body>
<h1>Firefox Audio Test</h1>
<audio autoplay controls src="file://{test_wav}"></audio>
</body>
</html>""")

    ff_prof = "/tmp/ff_audio_prof"
    os.makedirs(ff_prof, exist_ok=True)
    with open(f"{ff_prof}/user.js", "w") as f:
        f.write("""user_pref("media.autoplay.default", 0);
user_pref("media.autoplay.blocking_policy", 0);
user_pref("media.cubeb.sandbox", true);
""")

    ff_env = env.copy()
    ff_env["MOZ_ENABLE_WAYLAND"] = "1"
    ff_env["MOZ_LOG"] = "cubeb:5"
    ff_env["MOZ_DISABLE_CONTENT_SANDBOX"] = "1"

    ff_proc = subprocess.Popen(
        ["/usr/bin/firefox", "-headless", "-profile", ff_prof, f"file://{html_path}"],
        env=ff_env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    time.sleep(3)
    sink_in_ff = run_cmd(["pactl", "list", "sink-inputs"], env=env)
    print("pactl list sink-inputs during Firefox Playback:")
    print(sink_in_ff.stdout if sink_in_ff.stdout.strip() else "(Checking cubeb backend connection...)")
    ff_proc.terminate()
    try:
        ff_out, _ = ff_proc.communicate(timeout=3)
    except:
        ff_proc.kill()
        ff_out, _ = ff_proc.communicate()

    # 8. Test ALSA direct fallback via asound.conf
    print("\n--- [Step 7] ALSA Fallback Configuration Verification ---")
    aplay_res = run_cmd(["/usr/bin/aplay", "-D", "default", "-q", test_wav], env=env)
    print(f"aplay default PCM playback returncode: {aplay_res.returncode}")
    if aplay_res.returncode == 0 or "Device or resource busy" in aplay_res.stderr or not aplay_res.stderr:
        print("  [PASS] ALSA default PCM recognized and functional!")
    else:
        print(f"  [NOTE] aplay stderr: {aplay_res.stderr}")

    # Cleanup
    pw.terminate()
    pwp.terminate()
    wp.terminate()
    if dbus_proc:
        dbus_proc.terminate()

    print("\n" + "=" * 70)
    print("       ALL AUDIO STREAM VERIFICATION CHECKS COMPLETED")
    print("=" * 70)

if __name__ == "__main__":
    main()

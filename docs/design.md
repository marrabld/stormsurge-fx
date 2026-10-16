# Design questions before implementation

## Agreed direction

- StormSurge FX is the project; Undertow is the first delay.
- Learn by making an effect worth playing through, rather than replicating a suite.
- Bitwig on Linux is the primary initial DAW. Standalone use is also wanted.
- Windows is a planned destination, so platform-dependent code stays outside DSP.
- GPL-3.0-only project; DPF is the approved framework for the build foundation.
- The pass-through build foundation and GUI mockup are authorised and done, and
  implementing Undertow's delay engine is now authorised too. Publishing a release
  still is not.
- Dark rhythmic repeats and stereo ping-pong are the initial musical direction;
  tempo sync belongs in the first playable preview, alongside millisecond timing.

## Listening target

The maintainer identified [Devin Townsend's Deadhead](https://www.youtube.com/watch?v=DrDM6V08wcE)
as the ambience reference. Use it for listening, not copied audio/assets or an
exact-reproduction claim. A quoted internet recipe describes an 800 ms mono delay
feeding long reverb and a stereo ping-pong delay. That attribution is unverified;
percentage controls are not comparable across devices and are not safe presets.

Start with a dark delay that can become a ping-pong effect. Keep a long reverb,
the multi-effect wet chain, ducking, compression and shimmer outside the first
preview. Preserve a distinct dry path. The goal is clear riff attacks with space
for repeats between phrases, not every part of the reference sound at once.

## Proposed boundaries

1. **DSP modules:** process supplied buffers using prepared state and explicit
   parameters. No host, window, device or filesystem dependencies.
2. **Plugin integration:** channel layouts, host automation, parameter/state
   persistence, tempo, latency and tail reporting.
3. **Standalone integration:** device lifecycle, routing, settings and error handling.
4. **Interface:** controls and meters communicating without blocking audio processing.

These are logical responsibilities, not instructions to create a large framework.
Extract only what the first effect actually needs.

## Proposed defaults — awaiting maintainer approval

These are proposed answers to the open design questions, not settled decisions.
Approve, amend or reject each one. The maintainer need not write technical
specifications; each entry states the tradeoff so it can be judged by ear later.

**Clean or coloured repeats**
Intentionally coloured, via a one-pole darkening lowpass. No tape or analogue
emulation claims. Coloured repeats sit better under dark riffs and mask themselves
less as feedback rises; clean repeats are more forgiving to start with and easier to
hear as a distinct effect.

**Time-change sound**
Read/write pointer crossfade, roughly 50 ms, restarted on each change. No pitch
glide in the first version. A crossfade never shifts pitch, so synced repeats stay
in tune and automation stays predictable; the cost is a brief level dip during a fast
sweep. Glide sounds more analogue but smears pitch on every automation move, which
matters more once tempo sync is also in play.

**Time range and taper**
20–2000 ms, default 400 ms, with a non-linear taper. The low end covers flam and
slapback at slow tempi; the top end covers the long ambience tail the roadmap wants
later without extra buffers. A taper matters more than the range: in a linear law the
300–600 ms region Undertow actually lives in gets most of the knob's travel, so spend
resolution there.

**Mix law**
Equal-gain linear blend, `out = dry·(1−mix) + wet·mix`, with no −3 dB centre dip.
Linear keeps dry attack at full level until it is turned down. Equal-power would hold
perceived level steady but pulls the dry down at centre, thinning the riff exactly
where Undertow should be adding space.

**Feedback topology and ceiling**
Single loop: delay output → feedback gain → tone filter → back to the delay input.
Feedback is smoothed continuously and capped at **0.90**, not 1.0. Bounding below
unity alone does not make the loop safe, because the tone filter sits inside it — any
passband gain above unity would push real loop gain past the nominal setting. Hence
the three-part rule: one-pole filter (unity at DC, never boosts), 0.90 ceiling, and a
numerical test at worst case.

**Tone placement**
Inside the feedback loop, so each repeat is darker than the last. This gives the
cumulative darkening the direction calls for and helps contain feedback. Outside the
loop would keep every repeat equally bright: cleaner to articulate, less atmospheric.

**Ping-pong topology and stereo input**
Full cross-coupling — 100% into the opposite channel, 0% self-feedback. True ping-pong
needs each side to feed only the other or it collapses to mono. The delay input is the
mono sum (L+R) written to both lines, so the first repeat is centred and the
alternation does the spatial work; dry is passed per channel, untouched. Watch for
level drift between sides at high feedback — the ceiling and tone filter should hold
it, but that is the first thing to check in playtesting.

**Bypass and tail**
Bypass ramps the wet path out over roughly 20 ms and lets the line keep running, so
the tail decays under its own feedback. Re-engaging resumes the existing tail instead
of restarting from silence. Reset only on sample-rate or transport change, or an
explicit clear. Report tail length as the maximum delay time so hosts keep processing
after the note stops. A hard cut would avoid clicks but throws away a musical tail.

Note the safety interaction: a ringing tail is the *opposite* of the standalone
monitor gate's fail-safe mute. Keep the two mechanisms clearly separate — bypassing
the effect must never unmute the monitor.

**Output headroom**
No internal limiter, no clipping, no automatic gain. Output level −∞…+6 dB, default
0 dB, with a documented recommendation to start monitoring low. A limiter would feel
safer but would hide instability and alter the sound. With feedback capped and a
non-boosting filter the loop is provably stable, so the remaining peak risk is just
dry and wet summing.

**Tempo sync**
Divisions: 1/2, dotted 1/4, 1/4, dotted 1/8, 1/8, dotted 1/16, 1/16. The long
divisions give the ambience character; the short ones give rhythmic repeats. When
tempo is missing or the transport is stopped, fall back to free milliseconds and show
that state in the UI rather than silently holding a stale tempo.

Bounding feedback below unity alone does not guarantee safe levels for arbitrary
input. Filters, interpolation and parameter transitions also need analysis and
tests. Don't introduce clipping or limiting silently to hide an unstable design.

## Technology decision

C++20/CMake with DPF is now selected, with the maintainer's approval after the
licence check. Current JUCE's open-source terms are AGPLv3 rather than plain GPLv3.
See [the decision](technology.md) and [build instructions](building.md) for pinned
dependencies and the initial JACK-only standalone limitations. No embedded Python
is needed in the production real-time callback.

Bitwig support means testing the actual supported host/version. Neither a
successful build nor a plugin validator proves hardware monitoring works.

## Next discussion

Approve or amend the proposed defaults above; they become the spec implementation
follows. Use [issue #1](https://github.com/marrabld/stormsurge-fx/issues/1) for that
discussion. Bitwig **6.0.11** is confirmed; interface details, preferred live versus
recorded workflow and a permitted dry riff are still to be supplied.
The maintainer need not write technical specifications; explain each musical
tradeoff before asking for a decision.

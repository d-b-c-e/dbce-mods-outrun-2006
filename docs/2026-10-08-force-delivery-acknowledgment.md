# Refused force delivery must not become accepted state

Source review for muted recording found that the runtime ignored the return from
`SetDeviceForcesXY` and advanced `prevConstantLevel` even when the native layer
refused the command. The deadband could then suppress a later retry of that force,
and the calculation recorder would report misleading admission feedback. Periodic
update refusals were also ignored.

The candidate checks both returns. A permanent refusal releases the retained force device
and its effects, reports an error and latches initialization until
an explicit Refresh/selection. Independent remap input handles survive native
`FreeDirectInput`. The refused constant request does not enter the previous-force
cache. An invalid/nonfinite output setting is refused before integer conversion.
Calculation state and logical route identifiers remain intact through the
sample; the initialized flag blocks every later native send. Resetting history
or slots inside a sink would change the remainder of the recorded calculation.
The existing explicit selection/Refresh path performs the reset before reacquiring.
Refresh now uses that same serialized selection-reset path, releasing retained
shared-model objects before creating replacements. Automatic device-list
population on the first FFB-page draw does not clear a failed-output latch.

Silencing an already initialized device no longer requires actuator readiness:
zero constant/periodic commands remain possible after readiness loss. Nonzero
requests still require readiness, and no zero path loads or acquires a device.
A permanently refused zero follows the same release path, rather than claiming
the wheel was silenced. Lifecycle admission/drain and exit finalization are unchanged.

Claude's independent review found that the first candidate would also latch
after ordinary Alt-Tab access loss. That candidate was held uninstalled. The
correction distinguishes `DIERR_NOTEXCLUSIVEACQUIRED`, `DIERR_INPUTLOST`,
`DIERR_NOTACQUIRED`, and the native's documented effect recreation
(`E_HANDLE` / `DIERR_NOTDOWNLOADED`). These initiate best-effort silence and a
pending recovery state, without changing the calculation's state mid-sample.
No further nonzero command reaches native while recovery is pending. At the
next foreground-eligible game update, the constant channel and both owned
periodic channels must each acknowledge zero before the ordinary warmup restarts.
Failed zeros are never recorded as accepted zeros. Background/menu time does
not consume the two-second recovery deadline; an unresolved foreground recovery
releases and latches. A successful channel cannot reset another channel's
deadline. This resumes normal Alt-Tab recovery without an unbounded retry or
replaying retained nonzero parameters with `StartEffect`.

The accepted output path and force arithmetic are unchanged. Recovery's neutral
handshake happens before the next calculation, whose initial-state checkpoint
therefore includes the reset. Recorded requests are still software requests,
not measured torque or a complete native-delivery trace.

The production-linked fake ABI fixture exercises accepted nonzero then refused
constant, periodic and zero sends; retained-force release; unchanged admission
cache; no automatic retry; explicit recovery; nonzero suppression with zero
admitted after readiness loss; and nonfinite strength/frequency refusal. The full
wheel/input fixture, consumer/host lifecycle suite and existing signal golden plus
record/replay suite and x86 Release build pass. Final logs:
`build/delivery-ack-wheel-v4.log`, `build/delivery-ack-lifecycle-v4.log`,
`build/delivery-ack-signal-v4.log`, `build/delivery-ack-build.log`.

Additional production-sink recorder tests refuse a constant send or the first
periodic send mid-sample: the device is released once, later native sends are
blocked, all three calculated requests retain their original slot identifiers,
and the saved sample's exact requests and post-state replay through the actual
calculation. A first internal candidate reset those fields during the sink;
that would violate the capture contract, so it was corrected before installation.
Compiling the new recorder fixture against that first source (`18b32ad`) fails
with `producer requests lost after refusal`; the corrected source passes the
same case. Negative evidence: `build/delivery-ack-negative-18b32ad`.

That source checkpoint was subsequently superseded by the reviewed and installed
candidate below.
No game, real device or force was used by these fixtures. Producer-preserving
muted capture and stage playback are still unfinished; this closes a prerequisite
in the delivery/recording contract, not those features.

The recovery follow-up passes the production wheel/input fixture, lifecycle
fixture, exact signal recorder/replayer and x86 Release build. Fake access and
effect loss cases cover a long background interval, failed neutral handshakes,
successful return with a restarted ramp, nonzero suppression, and a persistent
auxiliary failure expiring at two seconds despite accepted constant zeros.
Immediate and deferred lifecycle silence both pause the deadline, including
focus loss where no more game updates arrive until the window returns.
Evidence: `build/delivery-recovery-wheel3.log`,
`build/delivery-recovery-lifecycle3.log`, `build/delivery-recovery-signal.log`,
`build/delivery-recovery-build2.log`. Physical focus recovery remains untested.

## Reviewed private installation, October 8 05:58 CT

Claude's final cross-review of `03bf2bd` plus `e5b44f8` passed. Its medium
focus-loss deadline finding is closed; a low note remains that background
updates can repeat best-effort zero attempts while recovery is pending. No
accepted-zero or physical-stop guarantee is inferred from those attempts.

Clean runtime `e5b44f80f0c028e4c22744c98636f6e499f3da6e` was rebuilt and packaged
with `-ReviewOnly`. Actual Windows PowerShell 5.1 and batch installer routes pass,
including retention, restore and rollback. ZIP SHA-256:
`1539E13A388F9A4B0B870E5095CC091D4910BD2A5A82E10D59E7D8326BF5DFD4`.
Installed proxy: `B2D9B6848F53F817D4F025B9581F2619F9AA3B307E420A5BE035D591C6AA3F66`.
The reviewed 41-export x86 native remains `432727681B1A8A6F62D68F2E1ED865FF9F229AECCC628B82FA97147626451968`.

Installation held the shared lease with the game closed, verified both runtime
hashes and preserved all five existing INI files. Evidence:
`build/delivery-e5b44f8-install/verified.json`; installer backup:
`.wheel-settings-backups/20261008-105836-838-f8168d18` in the existing Stream Deck
game folder. No launch or physical force test was performed for this installation.
Physical focus recovery, original muted force capture and route playback remain
open.

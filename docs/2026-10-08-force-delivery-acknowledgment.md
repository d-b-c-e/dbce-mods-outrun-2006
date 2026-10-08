# Refused force delivery must not become accepted state

Source review for muted recording found that the runtime ignored the return from
`SetDeviceForcesXY` and advanced `prevConstantLevel` even when the native layer
refused the command. The deadband could then suppress a later retry of that force,
and the calculation recorder would report misleading admission feedback. Periodic
update refusals were also ignored.

The candidate checks both returns. A refusal releases the retained force device
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
A refused zero follows the same release path, rather than claiming the wheel was
silenced. Lifecycle admission/drain and exit finalization are unchanged.

This deliberately also latches after a transient refusal which the native library
might otherwise retry. It favors an explicit reported interruption over continuing
with uncertain delivery. The accepted output path and force arithmetic are
unchanged. Recorded requests are still software requests, not measured torque.

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

Source candidate only at this checkpoint: independent review, clean package and
installation remain pending. Installed native-maintenance `604d5d7` is preserved.
No game, real device or force was used by these fixtures. Producer-preserving
muted capture and stage playback are still unfinished; this closes a prerequisite
in the delivery/recording contract, not those features.

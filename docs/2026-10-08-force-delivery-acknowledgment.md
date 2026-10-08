# Refused force delivery must not become accepted state

Source review for muted recording found that the runtime ignored the return from
`SetDeviceForcesXY` and advanced `prevConstantLevel` even when the native layer
refused the command. The deadband could then suppress a later retry of that force,
and the calculation recorder would report misleading admission feedback. Periodic
update refusals were also ignored.

The candidate checks both returns. A refusal releases the retained force device,
disables its auxiliary slots, reports an error and latches initialization until
an explicit Refresh/selection. Independent remap input handles survive native
`FreeDirectInput`. The refused constant request does not enter the previous-force
cache. An invalid/nonfinite output setting is refused before integer conversion.

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
record/replay suite pass. Logs: `build/delivery-ack-wheel-check.log`,
`build/delivery-ack-lifecycle.log`, `build/delivery-ack-signal.log`.

Source candidate only at this checkpoint: independent review, rebuild/package and
installation remain pending. Installed native-maintenance `604d5d7` is preserved.
No game, real device or force was used by these fixtures. Producer-preserving
muted capture and stage playback are still unfinished; this closes a prerequisite
in the delivery/recording contract, not those features.

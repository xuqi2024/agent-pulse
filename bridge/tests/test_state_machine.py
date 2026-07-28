"""Tests for the state machine merging + debounce."""
import time
from agent_pulse.state_machine import (
    StateMachine, SourceEvent, DEBOUNCE_PROC_MS, DEBOUNCE_IDLE_MS,
)


def _ms() -> int:
    return int(time.time() * 1000)


def test_initial_state_is_idle():
    sm = StateMachine()
    rs = sm.current()
    assert rs.status == "idle"


def test_higher_priority_source_wins():
    sm = StateMachine()
    sm.submit(SourceEvent(source="cursor",  status="processing", tool="cursor"))
    sm.submit(SourceEvent(source="file",    status="idle"))
    rs = sm.current()
    assert rs.status == "processing"
    assert rs.tool == "cursor"


def test_lower_priority_idle_does_not_overwrite_processing():
    sm = StateMachine()
    sm.submit(SourceEvent(source="file", status="processing", tool="Bash"))
    sm.submit(SourceEvent(source="cursor", status="idle"))
    rs = sm.current()
    # file is processing -> still processing
    assert rs.status == "processing"
    assert rs.tool == "Bash"


def test_processing_must_see_end_to_go_idle():
    sm = StateMachine()
    sm.submit(SourceEvent(source="file", status="processing"))
    assert sm.current().status == "processing"
    sm.submit(SourceEvent(source="file", status="idle"))
    # After the same source emits idle, it should clear.
    rs = sm.current()
    assert rs.status == "idle"


def test_dedupe_identical_events():
    sm = StateMachine()
    sm.submit(SourceEvent(source="file", status="processing", tool="Bash"))
    before = sm.snapshot()
    sm.submit(SourceEvent(source="file", status="processing", tool="Bash"))
    after = sm.snapshot()
    assert before == after


def test_debounce_holds_for_processing():
    sm = StateMachine()
    sm.submit(SourceEvent(source="file", status="processing", tool="Bash"))
    # First tick: pending should be set, but emit is still the default
    rs0 = sm.tick(now_ms=_ms())
    assert rs0.status == "idle"
    # Within debounce window, still idle
    rs1 = sm.tick(now_ms=_ms() + DEBOUNCE_PROC_MS // 2)
    assert rs1.status == "idle"
    # Past debounce, emit
    rs2 = sm.tick(now_ms=_ms() + DEBOUNCE_PROC_MS + 50)
    assert rs2.status == "processing"


def test_debounce_idle_longer():
    sm = StateMachine()
    sm.submit(SourceEvent(source="file", status="processing", tool="Bash"))
    sm.tick(now_ms=0)
    sm.tick(now_ms=DEBOUNCE_PROC_MS + 1)
    # Now switch to idle. The next tick will reset the debounce timer
    # (the pending record is updated when the new event is observed).
    sm.submit(SourceEvent(source="file", status="idle"))
    # First tick after the submit — pending reset to idle, debounce starts now.
    rs = sm.tick(now_ms=DEBOUNCE_PROC_MS + 1 + DEBOUNCE_IDLE_MS - 50)
    assert rs.status == "processing"
    # Second tick 850ms later — past the idle debounce window.
    rs2 = sm.tick(now_ms=DEBOUNCE_PROC_MS + 1 + DEBOUNCE_IDLE_MS + 850)
    assert rs2.status == "idle"

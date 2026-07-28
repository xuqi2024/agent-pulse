"""Tests for the protocol codec."""
import json
from agent_pulse.protocol import Hello, State, Config, parse_line


def test_hello_roundtrip():
    h = Hello(device_name="xq", width=320, height=240)
    s = h.to_json()
    assert s.endswith("\n")
    j = json.loads(s)
    assert j["t"] == "hello"
    assert j["device_name"] == "xq"
    assert j["width"] == 320
    assert j["height"] == 240


def test_state_to_json_omits_none():
    s = State(status="idle", tool=None, message=None, progress=None, seq=1)
    j = s.to_json()
    assert "tool" not in j
    assert "message" not in j
    assert "progress" not in j
    assert '"status":"idle"' in j
    assert '"seq":1' in j


def test_state_to_json_includes_all():
    s = State(status="processing", tool="Bash", message="echo hi",
              progress=42, seq=7)
    j = s.to_json()
    assert '"status":"processing"' in j
    assert '"tool":"Bash"' in j
    assert '"message":"echo hi"' in j
    assert '"progress":42' in j
    assert '"seq":7' in j


def test_config_to_json_omits_none():
    c = Config(brightness=80, theme=None, screen_rotation=None,
               idle_animation=None, fps_cap=None)
    j = c.to_json()
    assert '"brightness":80' in j
    assert "theme" not in j
    assert "fps_cap" not in j


def test_parse_line_rejects_non_json():
    assert parse_line("") is None
    assert parse_line("not json") is None
    assert parse_line('{"foo":"bar"}') is None  # unknown type


def test_parse_line_accepts_known_inbound():
    assert parse_line('{"t":"hello_ack","fw":1}')["t"] == "hello_ack"
    assert parse_line('{"t":"pong","seq":2}')["t"] == "pong"
    assert parse_line('{"t":"state_ack","seq":3,"rendered":true}')["t"] == "state_ack"
    assert parse_line('{"t":"btn","name":"boot"}')["t"] == "btn"
    assert parse_line('{"t":"error","code":"X"}')["t"] == "error"

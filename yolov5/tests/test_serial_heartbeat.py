from serial_heartbeat import should_send_heartbeat, should_keep_target


def test_should_not_send_before_interval():
    assert should_send_heartbeat(last_send_time=10.0, now=10.03, interval=0.05) is False


def test_should_send_after_interval():
    assert should_send_heartbeat(last_send_time=10.0, now=10.05, interval=0.05) is True


def test_should_keep_target_within_hold_window():
    assert should_keep_target(last_target_time=10.0, now=10.2, hold_seconds=0.25) is True


def test_should_not_keep_target_after_hold_window():
    assert should_keep_target(last_target_time=10.0, now=10.3, hold_seconds=0.25) is False


def test_should_not_keep_when_never_seen():
    assert should_keep_target(last_target_time=None, now=10.0, hold_seconds=0.25) is False

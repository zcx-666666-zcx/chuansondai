from serial_heartbeat import should_send_heartbeat


def test_interval_not_reached_should_not_send():
    assert should_send_heartbeat(last_send_time=10.0, now=10.03, interval=0.05) is False


def test_interval_reached_should_send():
    assert should_send_heartbeat(last_send_time=10.0, now=10.05, interval=0.05) is True

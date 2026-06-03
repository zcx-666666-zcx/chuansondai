def should_send_heartbeat(last_send_time, now, interval):
    return (now - last_send_time) >= interval


def should_keep_target(last_target_time, now, hold_seconds):
    """Keep sending R briefly after the last valid frame to avoid servo hunting."""
    if last_target_time is None:
        return False
    return (now - last_target_time) < hold_seconds

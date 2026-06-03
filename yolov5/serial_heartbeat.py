def should_send_heartbeat(last_send_time, now, interval):
    return (now - last_send_time) >= interval

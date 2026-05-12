-- migrate:up
CREATE TRIGGER touch_ticket_updated_at_after_activity_insert
AFTER INSERT ON helpdesk_ticket_activity
FOR EACH ROW
BEGIN
    UPDATE helpdesk_ticket
       SET updated_at = CURRENT_TIMESTAMP
     WHERE id = NEW.ticket_id;
END;

CREATE TRIGGER touch_ticket_updated_at_after_activity_update
AFTER UPDATE ON helpdesk_ticket_activity
FOR EACH ROW
BEGIN
    UPDATE helpdesk_ticket
       SET updated_at = CURRENT_TIMESTAMP
     WHERE id = NEW.ticket_id;
END;

-- migrate:down
DROP TRIGGER IF EXISTS touch_ticket_updated_at_after_activity_update;
DROP TRIGGER IF EXISTS touch_ticket_updated_at_after_activity_insert;

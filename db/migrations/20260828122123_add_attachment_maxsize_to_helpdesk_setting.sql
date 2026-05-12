-- migrate:up
ALTER TABLE helpdesk_setting
  ADD COLUMN ticket_attachment_maxsize INTEGER NOT NULL DEFAULT 47185920;
ALTER TABLE helpdesk_setting
  ADD COLUMN ticket_activity_attachment_maxsize INTEGER NOT NULL DEFAULT 15728640;

-- migrate:down
ALTER TABLE helpdesk_setting DROP COLUMN ticket_activity_attachment_maxsize;
ALTER TABLE helpdesk_setting DROP COLUMN ticket_attachment_maxsize;

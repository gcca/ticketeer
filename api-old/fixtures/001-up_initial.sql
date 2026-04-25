INSERT INTO ticketeer.helpdesk_ticket_status (name, display_name, trait) VALUES
    ('unassigned',   'Sin asignar', 'open'),
    ('assigned',     'Asignado',    'open'),
    ('working',      'En trabajo',  'in_progress'),
    ('stopped',      'Detenido',    'in_progress'),
    ('solved',       'Resuelto',    'closed'),
    ('not_proceeds', 'No procede',  'closed'),
    ('cancelled',    'Cancelado',   'closed');

INSERT INTO ticketeer.helpdesk_priority (name, display_name) VALUES
    ('low',      'Baja'),
    ('medium',   'Media'),
    ('high',     'Alta'),
    ('critical', 'Crítica');

INSERT INTO ticketeer.helpdesk_request_category (name) VALUES
    ('incident'),
    ('service_request');

INSERT INTO ticketeer.helpdesk_setting (default_status_id, assigned_status_id)
VALUES (
    (SELECT id FROM ticketeer.helpdesk_ticket_status WHERE name = 'unassigned'),
    (SELECT id FROM ticketeer.helpdesk_ticket_status WHERE name = 'assigned')
);

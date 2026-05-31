INSERT INTO auth_user (username, password, name, email) VALUES
    ('system', '!', 'System', '');

INSERT INTO helpdesk_ticket_status (name, display_name, trait) VALUES
    ('unassigned',   'Sin asignar', 'open'),
    ('assigned',     'Asignado',    'open'),
    ('working',      'En trabajo',  'in_progress'),
    ('stopped',      'Detenido',    'in_progress'),
    ('solved',       'Resuelto',    'closed'),
    ('not_proceeds', 'No procede',  'closed'),
    ('cancelled',    'Cancelado',   'closed');

INSERT INTO helpdesk_priority (name, display_name) VALUES
    ('low',      'Baja'),
    ('medium',   'Media'),
    ('high',     'Alta'),
    ('critical', 'Crítica');

INSERT INTO helpdesk_profile (user_id, role) VALUES
    ((SELECT id FROM auth_user WHERE username = 'system'), 'system');

INSERT INTO helpdesk_setting
    (name, default_status_id, default_assigned_to_id, default_ticket_priority_id, assigned_status_id, system_profile_id, ticket_body_maxlength, ticket_activity_body_maxlength, ticket_due_delta)
VALUES (
    'default',
    (SELECT id FROM helpdesk_ticket_status WHERE name = 'unassigned'),
    (SELECT id FROM helpdesk_profile WHERE role = 'system'),
    (SELECT id FROM helpdesk_priority WHERE name = 'medium'),
    (SELECT id FROM helpdesk_ticket_status WHERE name = 'assigned'),
    (SELECT id FROM helpdesk_profile WHERE role = 'system'),
    570,
    170,
    172800
);

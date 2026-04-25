INSERT INTO helpdesk.ticket_status (name, display_name, trait) VALUES
    ('unassigned',   'Sin asignar', 'open'),
    ('assigned',     'Asignado',    'open'),
    ('working',      'En trabajo',  'in_progress'),
    ('stopped',      'Detenido',    'in_progress'),
    ('solved',       'Resuelto',    'closed'),
    ('not_proceeds', 'No procede',  'closed'),
    ('cancelled',    'Cancelado',   'closed');

INSERT INTO helpdesk.priority (name, display_name) VALUES
    ('low',      'Baja'),
    ('medium',   'Media'),
    ('high',     'Alta'),
    ('critical', 'Crítica');

INSERT INTO helpdesk.request_category (name) VALUES
    ('incident'),
    ('service_request');

INSERT INTO helpdesk.department (name, description) VALUES
    ('No Asociado', 'Departamento por defecto para tickets sin área asociada');

INSERT INTO auth."user" (username, password) VALUES
    ('system', crypt('t801', gen_salt('bf')));

INSERT INTO helpdesk.profile (user_id, department_id, role) VALUES
    (
        (SELECT id FROM auth."user" WHERE username = 'system'),
        (SELECT id FROM helpdesk.department WHERE name = 'No Asociado'),
        'system'
    );

INSERT INTO helpdesk.setting
    (default_status_id, assigned_status_id, default_department_id, system_user_id)
VALUES (
    (SELECT id FROM helpdesk.ticket_status WHERE name = 'unassigned'),
    (SELECT id FROM helpdesk.ticket_status WHERE name = 'assigned'),
    (SELECT id FROM helpdesk.department WHERE name = 'No Asociado'),
    (SELECT id FROM helpdesk.profile WHERE role = 'system')
);

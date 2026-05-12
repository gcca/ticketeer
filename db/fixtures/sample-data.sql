DELETE FROM helpdesk_ticket_activity_attachment;
DELETE FROM helpdesk_ticket_activity;
DELETE FROM helpdesk_ticket;
DELETE FROM helpdesk_setting;
DELETE FROM helpdesk_priority;
DELETE FROM helpdesk_ticket_status;
DELETE FROM helpdesk_profile;
DELETE FROM auth_session;
DELETE FROM auth_user;
DELETE FROM auth_app_provider;

INSERT INTO auth_user (username, password, name, email) VALUES
    ('system',        'pbkdf2_sha256$600000$bf359be1cec54e26f601dc62501d18b4$4cd735b45be8b1d1312ab91ef5151dc3db5c127c9c64c4e4fdd3b0abe2849f75', 'System',           ''),
    ('administrator', 'pbkdf2_sha256$600000$d696a012ba29da26b56906ccd929e51b$4702095d1e618ee2cc0c2d40124f816bb0e69d2ff2f952981eb70a1e87fc3559', 'Albert Wesker',    'administrator@test.local'),
    ('supervisor01',  'pbkdf2_sha256$600000$8d2b537686c8ee9d958bb1e3a19856df$85bd67c1881f8efe5575e52b30cc521f55e7e6f9f1d3ba6f8e8c80999bcec013', 'Chris Redfield',   'supervisor01@test.local'),
    ('tech01',        'pbkdf2_sha256$600000$c9292282ab9728411c7ff1cf03899356$31348d0dfa012e17c481cb2d6fcdb250ae30ca739bd8724e2d5a4ca763a0d3a7', 'Jill Valentine',   'tech01@test.local'),
    ('tech02',        'pbkdf2_sha256$600000$75a86a4b80fbe129ef44f616e4d0e9d4$62e89c2e44f7bfde085a12faab4b9b8fdde788b262df67681ed0c5cdf21550e1', 'Barry Burton',     'tech02@test.local'),
    ('tech03',        'pbkdf2_sha256$600000$700d0e37e18266ec4a468c370ae6f10a$fce77b7cb366c50d95a3592f18d19578e2c30f880e264363042593d55a583fde', 'Rebecca Chambers', 'tech03@test.local'),
    ('tech04',        'pbkdf2_sha256$600000$b4a46e6aca46bb581f2b5fa24db431e0$2c06459afb2386bea803058719a527915bf41980230d4aa420ccd57201d41927', 'Brad Vickers',     'tech04@test.local'),
    ('requester01',   'pbkdf2_sha256$600000$e17245b3f08cfa8fbe8ea1b6e5a2f17f$6856c1035ab8cda931b7322d379c1b2e34604ec1562c33b2b9064ef585c39865', 'Enrico Marini',    'requester01@test.local'),
    ('requester02',   'pbkdf2_sha256$600000$9acab1f94ac2281efef514965c390faf$81cb52a5b9a600c9990d4715898961df51fd4757ddf07092be76778c802f16c0', 'Forest Speyer',    'requester02@test.local'),
    ('requester03',   'pbkdf2_sha256$600000$ba1b8626221b14ea42a923124ed25050$682896fe59fa17ab81641fd9116c362ddbadf9e08e39da3f11c78cee5f081882', 'Kenneth Sullivan', 'requester03@test.local'),
    ('requester04',   'pbkdf2_sha256$600000$bfbee027e832e86691aeef56d8005579$a2da471e97ca38bdbf65b1821a631f9b17a8e820e61d84bc1491464c6d7dd4df', 'Richard Aiken',    'requester04@test.local'),
    ('requester05',   'pbkdf2_sha256$600000$f3512e6df4471c0bd08b0a0777c09f2e$8a3240e548ad2a9462a33ddd5c6183afd50eef07159904aab0670afd02821b0d', 'Joseph Frost',     'requester05@test.local'),
    ('requester06',   'pbkdf2_sha256$600000$41e73fcf6eb8684887fccb2327c6d3bd$473c826eea0ea07df5089a30531c58c76643c83fe02f4ab5633f474a22745f30', 'Daniel Cortini',   'requester06@test.local'),
    ('requester07',   'pbkdf2_sha256$600000$266f03bcb300e2aac2f23ae67ca2c407$46c51668d9374a41d06dbb56562632af2d49fd2a9ecb7aac6cfcddf10cc51819', 'Lisa Trevor',      'requester07@test.local'),
    ('requester08',   'pbkdf2_sha256$600000$0e23307d855dbac6b29eb686a1ca69a7$9ca1fad4059a0d53a73995a0fe851252d0fce8199d54572a97fe3d1083e22a07', 'Zombie A',         'requester08@test.local'),
    ('requester09',   'pbkdf2_sha256$600000$f7064416db0ade6040e995b658748b7c$143a595a3612371486974f520f4e47ee59ba64e6c959b022b7827a5ea5dc01ea', 'Zombie B',         'requester09@test.local'),
    ('requester10',   'pbkdf2_sha256$600000$cd8b7de95bfb556865558444a090b2e8$c5d1f8053f2b5ea25e669bd76a2546c96ef842cc765b9d797521282d3805556d', 'Zombie C',         'requester10@test.local'),
    ('requester11',   'pbkdf2_sha256$600000$c31688dd88256f4cc4603d2aa02fb556$37add66103479eb52bdf55665be167e016419272913a5c63f0848e3a8741a86a', 'Zombie D',         'requester11@test.local'),
    ('requester12',   'pbkdf2_sha256$600000$1c588e258bacb139d1dae301e6334234$96b05254b94d9001242c7f26b79275bcd574a7fddfdfc5a6fc57b8f8601c1a50', 'Zombie E',         'requester12@test.local'),
    ('requester13',   'pbkdf2_sha256$600000$4babc9de8a4119104f2c4823c336ad46$994b0427e2b37f1dff073586f9954f3ebcb8f35bae0b35687524115c4e51759d', 'Zombie F',         'requester13@test.local'),
    ('requester14',   'pbkdf2_sha256$600000$1d274349058af9de7c212b5a774cff25$0adb34ee644e352f13b3604c38a2e587780277ae643a4e8b692f38bd1b5294c7', 'Zombie G',         'requester14@test.local'),
    ('requester15',   'pbkdf2_sha256$600000$d5dcbdeead169b8a303a1ddbefc39a3c$58160b67203fdc662dea53b39b865280b6748a492a0828e8bf479ff087343373', 'Zombie H',         'requester15@test.local');

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
    (2,  'administrator'),
    (3,  'supervisor'),
    (4,  'technician'),
    (5,  'technician'),
    (6,  'technician'),
    (7,  'technician'),
    (8,  'requester'),
    (9,  'requester'),
    (10, 'requester'),
    (11, 'requester'),
    (12, 'requester'),
    (13, 'requester'),
    (14, 'requester'),
    (15, 'requester'),
    (16, 'requester'),
    (17, 'requester'),
    (18, 'requester'),
    (19, 'requester'),
    (20, 'requester'),
    (21, 'requester'),
    (22, 'requester'),
    ((SELECT id FROM auth_user WHERE username = 'system'), 'system');

INSERT INTO helpdesk_setting
    (name, default_status_id, assigned_status_id, default_assigned_to_id, default_ticket_priority_id, system_profile_id, ticket_body_maxlength, ticket_activity_body_maxlength, ticket_due_delta)
VALUES (
    'default',
    (SELECT id FROM helpdesk_ticket_status WHERE name = 'unassigned'),
    (SELECT id FROM helpdesk_ticket_status WHERE name = 'assigned'),
    (SELECT id FROM helpdesk_profile WHERE role = 'supervisor'),
    (SELECT id FROM helpdesk_priority WHERE name = 'medium'),
    (SELECT id FROM helpdesk_profile WHERE role = 'system'),
    570,
    170,
    172800
);

INSERT INTO helpdesk_ticket
    (requester_id, assigned_to_id, priority_id, status_id,
     body, created_at, updated_at)
VALUES
    (7,  3,    3, 3, 'La laptop no enciende después de una actualización de BIOS.',                        datetime('now', '-28 days'), datetime('now', '-27 days')),
    (8,  4,    3, 5, 'Sin acceso a internet en toda la planta baja desde el lunes.',                       datetime('now', '-26 days'), datetime('now', '-20 days')),
    (9,  3,    2, 5, 'Usuario bloqueado en el sistema de nómina después de tres intentos fallidos.',        datetime('now', '-25 days'), datetime('now', '-22 days')),
    (10, NULL, 1, 1, 'Restablecer contraseña de correo corporativo.',                                      datetime('now', '-24 days'), datetime('now', '-24 days')),
    (11, 3,    1, 3, 'Instalar Adobe Acrobat Reader en el equipo de contabilidad.',                        datetime('now', '-23 days'), datetime('now', '-23 days')),
    (12, NULL, 2, 1, 'Solicitar un segundo monitor para el puesto de análisis financiero.',                datetime('now', '-22 days'), datetime('now', '-22 days')),
    (13, 7,    2, 2, 'No puede conectarse a VPN desde casa para acceder al ERP.',                          datetime('now', '-21 days'), datetime('now', '-21 days')),
    (14, NULL, 2, 1, 'La impresora del almacén imprime páginas en blanco.',                                datetime('now', '-20 days'), datetime('now', '-19 days')),
    (15, 5,    4, 2, 'El servidor de cámaras de seguridad no responde.',                                   datetime('now', '-19 days'), datetime('now', '-19 days')),
    (16, 6,    4, 3, 'Caída total de la red en el datacenter secundario.',                                 datetime('now', '-18 days'), datetime('now', '-17 days')),
    (17, 5,    3, 3, 'Acceso denegado al portal de administración de infraestructura.',                    datetime('now', '-17 days'), datetime('now', '-16 days')),
    (18, NULL, 1, 1, 'Cambio de contraseña obligatorio expirado.',                                         datetime('now', '-16 days'), datetime('now', '-16 days')),
    (19, 3,    1, 5, 'Instalar cliente VPN GlobalProtect en nuevo equipo.',                                datetime('now', '-15 days'), datetime('now', '-10 days')),
    (20, 4,    2, 5, 'Teclado y ratón inalámbrico para trabajo remoto.',                                   datetime('now', '-14 days'), datetime('now', '-8 days')),
    (21, NULL, 2, 1, 'VPN cae al intentar conectarse con token MFA.',                                      datetime('now', '-13 days'), datetime('now', '-13 days')),
    (7,  3,    2, 4, 'Impresora de recepción no reconocida tras reinstalación de Windows.',                datetime('now', '-12 days'), datetime('now', '-11 days')),
    (8,  4,    3, 3, 'Pantalla del all-in-one parpadea constantemente.',                                   datetime('now', '-11 days'), datetime('now', '-11 days')),
    (9,  7,    3, 2, 'Velocidad de red extremadamente lenta en piso 3.',                                   datetime('now', '-10 days'), datetime('now', '-10 days')),
    (10, 3,    2, 2, 'Usuario no puede acceder al módulo de vacaciones del HRMS.',                         datetime('now', '-9 days'),  datetime('now', '-9 days')),
    (11, NULL, 1, 1, 'Resetear PIN de token físico de autenticación.',                                     datetime('now', '-8 days'),  datetime('now', '-8 days')),
    (12, 4,    1, 5, 'Actualizar Office 365 a la última versión en equipo de auditoría.',                  datetime('now', '-7 days'),  datetime('now', '-4 days')),
    (13, NULL, 2, 1, 'Necesito una etiquetadora portátil para el almacén.',                                datetime('now', '-7 days'),  datetime('now', '-7 days')),
    (14, 6,    2, 3, 'VPN no levanta en macOS Sonoma después de actualización.',                           datetime('now', '-6 days'),  datetime('now', '-6 days')),
    (15, 5,    2, 5, 'Impresora de credenciales no imprime con el nuevo cartucho.',                        datetime('now', '-6 days'),  datetime('now', '-3 days')),
    (16, 6,    4, 3, 'Disco duro del servidor de respaldo emite ruido y falla SMART.',                     datetime('now', '-5 days'),  datetime('now', '-5 days')),
    (17, NULL, 4, 1, 'Enlace de fibra óptica intermitente en rack principal.',                             datetime('now', '-5 days'),  datetime('now', '-5 days')),
    (18, 5,    3, 3, 'Acceso SSH denegado al firewall perimetral.',                                        datetime('now', '-4 days'),  datetime('now', '-4 days')),
    (19, NULL, 1, 1, 'Contraseña de nuevo ingreso no funciona en el primer acceso.',                       datetime('now', '-4 days'),  datetime('now', '-4 days')),
    (20, 4,    1, 2, 'Instalar Slack en computadora de RRHH.',                                             datetime('now', '-3 days'),  datetime('now', '-3 days')),
    (21, NULL, 2, 1, 'Solicitar licencia de AutoCAD para el área de proyectos.',                           datetime('now', '-3 days'),  datetime('now', '-3 days')),
    (7,  3,    2, 3, 'VPN desconecta cada 30 minutos en conexión por celular.',                            datetime('now', '-2 days'),  datetime('now', '-2 days')),
    (8,  NULL, 2, 1, 'Impresora de sala de juntas sin papel y sin tóner.',                                 datetime('now', '-2 days'),  datetime('now', '-2 days')),
    (9,  NULL, 3, 1, 'Mouse del equipo de reclutamiento no funciona.',                                     datetime('now', '-1 day'),   datetime('now', '-1 day')),
    (10, 6,    3, 2, 'Red WiFi de sala de capacitación no aparece en dispositivos.',                       datetime('now', '-1 day'),   datetime('now', '-1 day')),
    (11, NULL, 2, 1, 'Error al iniciar sesión en el sistema de facturación SAP.',                          datetime('now', '-1 day'),   datetime('now', '-1 day')),
    (12, 4,    1, 3, 'Solicitar reseteo de contraseña del sistema de tesorería.',                          datetime('now', '-12 hours'), datetime('now', '-12 hours')),
    (13, NULL, 1, 1, 'Instalar actualizaciones pendientes en equipo de logística.',                        datetime('now', '-10 hours'), datetime('now', '-10 hours')),
    (14, 5,    2, 2, 'Solicitar cámara web HD para entrevistas remotas.',                                  datetime('now', '-8 hours'),  datetime('now', '-8 hours')),
    (15, NULL, 3, 1, 'No puedo acceder a la VPN después del cambio de contraseña de AD.',                  datetime('now', '-6 hours'),  datetime('now', '-6 hours')),
    (16, 6,    4, 3, 'Impresora de centro de datos imprimiendo caracteres corruptos.',                     datetime('now', '-5 hours'),  datetime('now', '-5 hours')),
    (17, NULL, 4, 1, 'UPS del rack C falla en prueba de autonomía.',                                       datetime('now', '-4 hours'),  datetime('now', '-4 hours')),
    (18, 5,    4, 3, 'Switch de core dropping paquetes de forma intermitente.',                            datetime('now', '-3 hours'),  datetime('now', '-3 hours')),
    (19, NULL, 2, 1, 'Cuenta de nuevo colaborador sin acceso a repositorio Git.',                          datetime('now', '-2 hours'),  datetime('now', '-2 hours')),
    (20, NULL, 1, 1, 'Contraseña expirada en portal de beneficios.',                                       datetime('now', '-90 minutes'), datetime('now', '-90 minutes')),
    (21, 3,    1, 2, 'Instalar drivers de impresora en equipo de contabilidad recién formateado.',         datetime('now', '-60 minutes'), datetime('now', '-60 minutes')),
    (7,  NULL, 3, 1, 'Teclado mecánico derrama líquido, teclas pegadas.',                                  datetime('now', '-45 minutes'), datetime('now', '-45 minutes')),
    (8,  6,    4, 2, 'Servidor de correo no responde desde hace 20 minutos.',                              datetime('now', '-30 minutes'), datetime('now', '-30 minutes')),
    (9,  NULL, 2, 1, 'Audífonos con micrófono para entrevistas por videollamada.',                         datetime('now', '-20 minutes'), datetime('now', '-20 minutes')),
    (10, NULL, 2, 1, 'Bloqueo de cuenta por intento de acceso desde IP desconocida.',                      datetime('now', '-15 minutes'), datetime('now', '-15 minutes')),
    (11, NULL, 2, 1, 'VPN empresarial no compatible con nuevo antivirus instalado por TI.',                datetime('now', '-10 minutes'), datetime('now', '-10 minutes')),
    (12, NULL, 3, 1, 'Impresora fiscal desconfigura formato al imprimir desde Excel.',                     datetime('now', '-5 minutes'),  datetime('now', '-5 minutes'));

INSERT INTO helpdesk_ticket_activity
    (ticket_id, profile_id, kind, body, metadata, created_at)
SELECT
    t.id,
    (SELECT id FROM helpdesk_profile WHERE role = 'system'),
    'status_changed',
    'status_changed: ' || ts.display_name,
    json_object('to_status_id', t.status_id, 'source', 'sample_data'),
    datetime(t.created_at, '+30 seconds')
FROM helpdesk_ticket t
JOIN helpdesk_ticket_status ts ON ts.id = t.status_id
WHERE t.id <= 10;

INSERT INTO helpdesk_ticket_activity
    (ticket_id, profile_id, kind, body, metadata, created_at)
SELECT
    t.id,
    t.requester_id,
    'message',
    'message: El problema empezó hoy y está afectando mi trabajo diario.',
    '{}',
    datetime(t.created_at, '+60 seconds')
FROM helpdesk_ticket t
WHERE t.id > 10 AND t.id <= 25;

INSERT INTO helpdesk_ticket_activity
    (ticket_id, profile_id, kind, body, metadata, created_at)
SELECT
    t.id,
    COALESCE(t.assigned_to_id, (SELECT id FROM helpdesk_profile WHERE role = 'supervisor')),
    'message',
    'message: Revisé la solicitud y continuaré con el diagnóstico técnico.',
    '{}',
    datetime(t.created_at, '+90 seconds')
FROM helpdesk_ticket t
WHERE t.id > 25 AND t.id <= 40;

INSERT INTO helpdesk_ticket_activity
    (ticket_id, profile_id, kind, body, metadata, created_at)
SELECT
    t.id,
    (SELECT id FROM helpdesk_profile WHERE role = 'system'),
    'assigned_changed',
    'assigned_changed: ' || u.name,
    json_object('to_profile_id', t.assigned_to_id, 'source', 'sample_data'),
    datetime(t.created_at, '+120 seconds')
FROM helpdesk_ticket t
JOIN helpdesk_profile p ON p.id = t.assigned_to_id
JOIN auth_user u        ON u.id = p.user_id
WHERE t.assigned_to_id IS NOT NULL AND t.id > 40;

INSERT INTO helpdesk_ticket_activity_attachment
    (ticket_activity_id, file_path, file_name, file_size, mime_type, created_at)
VALUES
    (1, 'role/ticket_id=1/activity_id=1/attachment_id=1',   'error_log.txt',      2048,    'text/plain',       datetime('now', '-20 days')),
    (2, 'role/ticket_id=2/activity_id=2/attachment_id=2',   'screenshot.png',     153600,  'image/png',        datetime('now', '-18 days')),
    (3, 'role/ticket_id=3/activity_id=3/attachment_id=3',   'manual.pdf',         2097152, 'application/pdf',  datetime('now', '-15 days')),
    (4, 'role/ticket_id=4/activity_id=4/attachment_id=4',   'config_backup.xlsx', 512000,  'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet', datetime('now', '-12 days')),
    (5, 'role/ticket_id=5/activity_id=5/attachment_id=5',   'photo.jpg',          1024000, 'image/jpeg',       datetime('now', '-10 days'));

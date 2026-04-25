TRUNCATE TABLE
    ticketeer.auth_session,
    ticketeer.helpdesk_ticket,
    ticketeer.helpdesk_setting,
    ticketeer.helpdesk_request_type,
    ticketeer.helpdesk_request_category,
    ticketeer.helpdesk_priority,
    ticketeer.helpdesk_ticket_status,
    ticketeer.helpdesk_profile,
    ticketeer.helpdesk_department,
    ticketeer.auth_user
RESTART IDENTITY CASCADE;

INSERT INTO ticketeer.auth_user (username, password) VALUES
    ('administrator',  crypt('t801', gen_salt('bf'))),
    ('supervisor01',   crypt('t801', gen_salt('bf'))),
    ('tech01',         crypt('t801', gen_salt('bf'))),
    ('tech02',         crypt('t801', gen_salt('bf'))),
    ('tech03',         crypt('t801', gen_salt('bf'))),
    ('tech04',         crypt('t801', gen_salt('bf'))),
    ('requester01',    crypt('t801', gen_salt('bf'))),
    ('requester02',    crypt('t801', gen_salt('bf'))),
    ('requester03',    crypt('t801', gen_salt('bf'))),
    ('requester04',    crypt('t801', gen_salt('bf'))),
    ('requester05',    crypt('t801', gen_salt('bf'))),
    ('requester06',    crypt('t801', gen_salt('bf'))),
    ('requester07',    crypt('t801', gen_salt('bf'))),
    ('requester08',    crypt('t801', gen_salt('bf'))),
    ('requester09',    crypt('t801', gen_salt('bf'))),
    ('requester10',    crypt('t801', gen_salt('bf'))),
    ('requester11',    crypt('t801', gen_salt('bf'))),
    ('requester12',    crypt('t801', gen_salt('bf'))),
    ('requester13',    crypt('t801', gen_salt('bf'))),
    ('requester14',    crypt('t801', gen_salt('bf'))),
    ('requester15',    crypt('t801', gen_salt('bf')));

INSERT INTO ticketeer.helpdesk_department (name, description) VALUES
    ('IT Support',         'Soporte técnico a usuarios finales'),
    ('Recursos Humanos',   'Gestión de personal y nómina'),
    ('Finanzas',           'Contabilidad, pagos y presupuesto'),
    ('Logística',          'Inventario, almacén y distribución'),
    ('Seguridad',          'Seguridad física y digital'),
    ('Infraestructura',    'Servidores, redes y telecomunicaciones');

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

INSERT INTO ticketeer.helpdesk_request_type
    (name, category_id, default_priority_id, description)
VALUES
    ('hardware_failure',      1, 3, 'Fallo físico de equipo de cómputo'),
    ('network_issue',         1, 3, 'Problemas de conectividad o red'),
    ('account_access',        1, 2, 'Problemas de acceso a cuenta o sistema'),
    ('password_reset',        2, 1, 'Restablecimiento de contraseña'),
    ('software_installation', 2, 1, 'Instalación o actualización de software'),
    ('equipment_request',     2, 2, 'Solicitud de equipo o periférico'),
    ('vpn_access',            2, 2, 'Solicitud o problema de acceso VPN'),
    ('printer_issue',         1, 2, 'Fallo o configuración de impresora');

INSERT INTO ticketeer.helpdesk_setting (name, default_status_id, assigned_status_id)
VALUES (
    'default',
    (SELECT id FROM ticketeer.helpdesk_ticket_status WHERE name = 'unassigned'),
    (SELECT id FROM ticketeer.helpdesk_ticket_status WHERE name = 'assigned')
);

INSERT INTO ticketeer.helpdesk_profile (user_id, department_id, role) VALUES
    (1,  1, 'administrator'),
    (2,  1, 'supervisor'),
    (3,  1, 'technician'),
    (4,  1, 'technician'),
    (5,  6, 'technician'),
    (6,  6, 'technician'),
    (7,  1, 'requester'),
    (8,  1, 'requester'),
    (9,  2, 'requester'),
    (10, 2, 'requester'),
    (11, 3, 'requester'),
    (12, 3, 'requester'),
    (13, 4, 'requester'),
    (14, 4, 'requester'),
    (15, 5, 'requester'),
    (16, 5, 'requester'),
    (17, 6, 'requester'),
    (18, 6, 'requester'),
    (19, 1, 'requester'),
    (20, 2, 'requester'),
    (21, 3, 'requester');

INSERT INTO ticketeer.helpdesk_ticket
    (request_type_id, requester_id, assigned_to_id, department_id,
     priority_id, status_id, description, created_at, updated_at)
VALUES
    (1, 7,  3, 1, 3, 3, 'La laptop no enciende después de una actualización de BIOS.',                        now()-'28 days'::interval, now()-'27 days'::interval),
    (2, 8,  4, 1, 3, 5, 'Sin acceso a internet en toda la planta baja desde el lunes.',                       now()-'26 days'::interval, now()-'20 days'::interval),
    (3, 9,  3, 2, 2, 5, 'Usuario bloqueado en el sistema de nómina después de tres intentos fallidos.',        now()-'25 days'::interval, now()-'22 days'::interval),
    (4, 10, NULL, 2, 1, 1, 'Restablecer contraseña de correo corporativo.',                                   now()-'24 days'::interval, now()-'24 days'::interval),
    (5, 11, 3, 3, 1, 3, 'Instalar Adobe Acrobat Reader en el equipo de contabilidad.',                        now()-'23 days'::interval, now()-'23 days'::interval),
    (6, 12, 4, 3, 2, 2, 'Solicitar un segundo monitor para el puesto de análisis financiero.',                 now()-'22 days'::interval, now()-'22 days'::interval),
    (7, 13, NULL, 4, 2, 1, 'No puede conectarse a VPN desde casa para acceder al ERP.',                       now()-'21 days'::interval, now()-'21 days'::interval),
    (8, 14, 4, 4, 2, 3, 'La impresora del almacén imprime páginas en blanco.',                                now()-'20 days'::interval, now()-'19 days'::interval),
    (1, 15, 5, 5, 4, 2, 'El servidor de cámaras de seguridad no responde.',                                   now()-'19 days'::interval, now()-'19 days'::interval),
    (2, 16, 6, 6, 4, 3, 'Caída total de la red en el datacenter secundario.',                                  now()-'18 days'::interval, now()-'17 days'::interval),
    (3, 17, 5, 6, 3, 3, 'Acceso denegado al portal de administración de infraestructura.',                    now()-'17 days'::interval, now()-'16 days'::interval),
    (4, 18, NULL, 2, 1, 1, 'Cambio de contraseña obligatorio expirado.',                                      now()-'16 days'::interval, now()-'16 days'::interval),
    (5, 19, 3, 1, 1, 5, 'Instalar cliente VPN GlobalProtect en nuevo equipo.',                                now()-'15 days'::interval, now()-'10 days'::interval),
    (6, 20, 4, 2, 2, 5, 'Teclado y ratón inalámbrico para trabajo remoto.',                                   now()-'14 days'::interval, now()-'8 days'::interval),
    (7, 21, NULL, 3, 2, 1, 'VPN cae al intentar conectarse con token MFA.',                                   now()-'13 days'::interval, now()-'13 days'::interval),
    (8, 7,  3, 1, 2, 4, 'Impresora de recepción no reconocida tras reinstalación de Windows.',                now()-'12 days'::interval, now()-'11 days'::interval),
    (1, 8,  4, 1, 3, 3, 'Pantalla del all-in-one parpadea constantemente.',                                   now()-'11 days'::interval, now()-'11 days'::interval),
    (2, 9,  NULL, 2, 3, 1, 'Velocidad de red extremadamente lenta en piso 3.',                                now()-'10 days'::interval, now()-'10 days'::interval),
    (3, 10, 3, 2, 2, 2, 'Usuario no puede acceder al módulo de vacaciones del HRMS.',                         now()-'9 days'::interval,  now()-'9 days'::interval),
    (4, 11, NULL, 3, 1, 1, 'Resetear PIN de token físico de autenticación.',                                  now()-'8 days'::interval,  now()-'8 days'::interval),
    (5, 12, 4, 3, 1, 5, 'Actualizar Office 365 a la última versión en equipo de auditoría.',                  now()-'7 days'::interval,  now()-'4 days'::interval),
    (6, 13, NULL, 4, 2, 1, 'Necesito una etiquetadora portátil para el almacén.',                             now()-'7 days'::interval,  now()-'7 days'::interval),
    (7, 14, 6, 4, 2, 3, 'VPN no levanta en macOS Sonoma después de actualización.',                           now()-'6 days'::interval,  now()-'6 days'::interval),
    (8, 15, 5, 5, 2, 5, 'Impresora de credenciales no imprime con el nuevo cartucho.',                        now()-'6 days'::interval,  now()-'3 days'::interval),
    (1, 16, 6, 6, 4, 3, 'Disco duro del servidor de respaldo emite ruido y falla SMART.',                     now()-'5 days'::interval,  now()-'5 days'::interval),
    (2, 17, NULL, 6, 4, 1, 'Enlace de fibra óptica intermitente en rack principal.',                          now()-'5 days'::interval,  now()-'5 days'::interval),
    (3, 18, 5, 5, 3, 3, 'Acceso SSH denegado al firewall perimetral.',                                        now()-'4 days'::interval,  now()-'4 days'::interval),
    (4, 19, NULL, 1, 1, 1, 'Contraseña de nuevo ingreso no funciona en el primer acceso.',                    now()-'4 days'::interval,  now()-'4 days'::interval),
    (5, 20, 4, 2, 1, 2, 'Instalar Slack en computadora de RRHH.',                                             now()-'3 days'::interval,  now()-'3 days'::interval),
    (6, 21, NULL, 3, 2, 1, 'Solicitar licencia de AutoCAD para el área de proyectos.',                        now()-'3 days'::interval,  now()-'3 days'::interval),
    (7, 7,  3, 1, 2, 3, 'VPN desconecta cada 30 minutos en conexión por celular.',                            now()-'2 days'::interval,  now()-'2 days'::interval),
    (8, 8,  NULL, 1, 2, 1, 'Impresora de sala de juntas sin papel y sin tóner.',                              now()-'2 days'::interval,  now()-'2 days'::interval),
    (1, 9,  NULL, 2, 3, 1, 'Mouse del equipo de reclutamiento no funciona.',                                  now()-'1 day'::interval,   now()-'1 day'::interval),
    (2, 10, 6, 2, 3, 2, 'Red WiFi de sala de capacitación no aparece en dispositivos.',                       now()-'1 day'::interval,   now()-'1 day'::interval),
    (3, 11, NULL, 3, 2, 1, 'Error al iniciar sesión en el sistema de facturación SAP.',                       now()-'1 day'::interval,   now()-'1 day'::interval),
    (4, 12, 4, 3, 1, 3, 'Solicitar reseteo de contraseña del sistema de tesorería.',                          now()-'12 hours'::interval, now()-'12 hours'::interval),
    (5, 13, NULL, 4, 1, 1, 'Instalar actualizaciones pendientes en equipo de logística.',                     now()-'10 hours'::interval, now()-'10 hours'::interval),
    (6, 14, 5, 5, 2, 2, 'Solicitar cámara web HD para entrevistas remotas.',                                  now()-'8 hours'::interval,  now()-'8 hours'::interval),
    (7, 15, NULL, 5, 3, 1, 'No puedo acceder a la VPN después del cambio de contraseña de AD.',               now()-'6 hours'::interval,  now()-'6 hours'::interval),
    (8, 16, 6, 6, 4, 3, 'Impresora de centro de datos imprimiendo caracteres corruptos.',                     now()-'5 hours'::interval,  now()-'5 hours'::interval),
    (1, 17, NULL, 6, 4, 1, 'UPS del rack C falla en prueba de autonomía.',                                    now()-'4 hours'::interval,  now()-'4 hours'::interval),
    (2, 18, 5, 6, 4, 3, 'Switch de core dropping paquetes de forma intermitente.',                            now()-'3 hours'::interval,  now()-'3 hours'::interval),
    (3, 19, NULL, 1, 2, 1, 'Cuenta de nuevo colaborador sin acceso a repositorio Git.',                       now()-'2 hours'::interval,  now()-'2 hours'::interval),
    (4, 20, NULL, 2, 1, 1, 'Contraseña expirada en portal de beneficios.',                                    now()-'90 minutes'::interval, now()-'90 minutes'::interval),
    (5, 21, 3, 3, 1, 2, 'Instalar drivers de impresora en equipo de contabilidad recién formateado.',         now()-'60 minutes'::interval, now()-'60 minutes'::interval),
    (1, 7,  NULL, 1, 3, 1, 'Teclado mecánico derrama líquido, teclas pegadas.',                               now()-'45 minutes'::interval, now()-'45 minutes'::interval),
    (2, 8,  6, 1, 4, 2, 'Servidor de correo no responde desde hace 20 minutos.',                              now()-'30 minutes'::interval, now()-'30 minutes'::interval),
    (6, 9,  NULL, 2, 2, 1, 'Audífonos con micrófono para entrevistas por videollamada.',                      now()-'20 minutes'::interval, now()-'20 minutes'::interval),
    (3, 10, NULL, 2, 2, 1, 'Bloqueo de cuenta por intento de acceso desde IP desconocida.',                   now()-'15 minutes'::interval, now()-'15 minutes'::interval),
    (7, 11, NULL, 3, 2, 1, 'VPN empresarial no compatible con nuevo antivirus instalado por TI.',             now()-'10 minutes'::interval, now()-'10 minutes'::interval),
    (8, 12, NULL, 3, 3, 1, 'Impresora fiscal desconfigura formato al imprimir desde Excel.',                  now()-'5 minutes'::interval,  now()-'5 minutes'::interval);

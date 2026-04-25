\restrict dbmate

-- Dumped from database version 18.3
-- Dumped by pg_dump version 18.3 (Homebrew)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET transaction_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

--
-- Name: auth; Type: SCHEMA; Schema: -; Owner: -
--

CREATE SCHEMA auth;


--
-- Name: common; Type: SCHEMA; Schema: -; Owner: -
--

CREATE SCHEMA common;


--
-- Name: helpdesk; Type: SCHEMA; Schema: -; Owner: -
--

CREATE SCHEMA helpdesk;


--
-- Name: pgcrypto; Type: EXTENSION; Schema: -; Owner: -
--

CREATE EXTENSION IF NOT EXISTS pgcrypto WITH SCHEMA public;


--
-- Name: EXTENSION pgcrypto; Type: COMMENT; Schema: -; Owner: -
--

COMMENT ON EXTENSION pgcrypto IS 'cryptographic functions';


--
-- Name: provider; Type: TYPE; Schema: auth; Owner: -
--

CREATE TYPE auth.provider AS ENUM (
    'overlord'
);


--
-- Name: role; Type: TYPE; Schema: helpdesk; Owner: -
--

CREATE TYPE helpdesk.role AS ENUM (
    'system',
    'administrator',
    'supervisor',
    'technician',
    'requester'
);


--
-- Name: ticket_activity_kind; Type: TYPE; Schema: helpdesk; Owner: -
--

CREATE TYPE helpdesk.ticket_activity_kind AS ENUM (
    'message',
    'status_changed',
    'assigned_changed',
    'department_changed',
    'priority_changed',
    'due_date_changed'
);


--
-- Name: ticket_trait; Type: TYPE; Schema: helpdesk; Owner: -
--

CREATE TYPE helpdesk.ticket_trait AS ENUM (
    'open',
    'in_progress',
    'closed'
);


--
-- Name: set_updated_at(); Type: FUNCTION; Schema: common; Owner: -
--

CREATE FUNCTION common.set_updated_at() RETURNS trigger
    LANGUAGE plpgsql
    AS $$
BEGIN
    NEW.updated_at = now();
    RETURN NEW;
END;
$$;


SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: session; Type: TABLE; Schema: auth; Owner: -
--

CREATE TABLE auth.session (
    id bigint NOT NULL,
    user_id bigint NOT NULL,
    token text NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL,
    expires_at timestamp with time zone NOT NULL
);


--
-- Name: session_id_seq; Type: SEQUENCE; Schema: auth; Owner: -
--

CREATE SEQUENCE auth.session_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: session_id_seq; Type: SEQUENCE OWNED BY; Schema: auth; Owner: -
--

ALTER SEQUENCE auth.session_id_seq OWNED BY auth.session.id;


--
-- Name: user; Type: TABLE; Schema: auth; Owner: -
--

CREATE TABLE auth."user" (
    id bigint NOT NULL,
    username text NOT NULL,
    password text NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL
);


--
-- Name: user_id_seq; Type: SEQUENCE; Schema: auth; Owner: -
--

CREATE SEQUENCE auth.user_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: user_id_seq; Type: SEQUENCE OWNED BY; Schema: auth; Owner: -
--

ALTER SEQUENCE auth.user_id_seq OWNED BY auth."user".id;


--
-- Name: useroauth; Type: TABLE; Schema: auth; Owner: -
--

CREATE TABLE auth.useroauth (
    id bigint NOT NULL,
    user_id bigint NOT NULL,
    provider_id bigint NOT NULL,
    provider auth.provider NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL
);


--
-- Name: useroauth_id_seq; Type: SEQUENCE; Schema: auth; Owner: -
--

CREATE SEQUENCE auth.useroauth_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: useroauth_id_seq; Type: SEQUENCE OWNED BY; Schema: auth; Owner: -
--

ALTER SEQUENCE auth.useroauth_id_seq OWNED BY auth.useroauth.id;


--
-- Name: department; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.department (
    id bigint NOT NULL,
    name text NOT NULL,
    description text DEFAULT ''::text NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL
);


--
-- Name: department_id_seq; Type: SEQUENCE; Schema: helpdesk; Owner: -
--

CREATE SEQUENCE helpdesk.department_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: department_id_seq; Type: SEQUENCE OWNED BY; Schema: helpdesk; Owner: -
--

ALTER SEQUENCE helpdesk.department_id_seq OWNED BY helpdesk.department.id;


--
-- Name: priority; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.priority (
    id bigint NOT NULL,
    name text NOT NULL,
    display_name text NOT NULL
);


--
-- Name: priority_id_seq; Type: SEQUENCE; Schema: helpdesk; Owner: -
--

CREATE SEQUENCE helpdesk.priority_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: priority_id_seq; Type: SEQUENCE OWNED BY; Schema: helpdesk; Owner: -
--

ALTER SEQUENCE helpdesk.priority_id_seq OWNED BY helpdesk.priority.id;


--
-- Name: profile; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.profile (
    id bigint NOT NULL,
    user_id bigint NOT NULL,
    department_id bigint NOT NULL,
    role helpdesk.role NOT NULL,
    name text NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL
);


--
-- Name: profile_id_seq; Type: SEQUENCE; Schema: helpdesk; Owner: -
--

CREATE SEQUENCE helpdesk.profile_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: profile_id_seq; Type: SEQUENCE OWNED BY; Schema: helpdesk; Owner: -
--

ALTER SEQUENCE helpdesk.profile_id_seq OWNED BY helpdesk.profile.id;


--
-- Name: request_category; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.request_category (
    id bigint NOT NULL,
    name text NOT NULL
);


--
-- Name: request_category_id_seq; Type: SEQUENCE; Schema: helpdesk; Owner: -
--

CREATE SEQUENCE helpdesk.request_category_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: request_category_id_seq; Type: SEQUENCE OWNED BY; Schema: helpdesk; Owner: -
--

ALTER SEQUENCE helpdesk.request_category_id_seq OWNED BY helpdesk.request_category.id;


--
-- Name: request_type; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.request_type (
    id bigint NOT NULL,
    name text NOT NULL,
    category_id bigint NOT NULL,
    default_priority_id bigint NOT NULL,
    description text DEFAULT ''::text NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL
);


--
-- Name: request_type_id_seq; Type: SEQUENCE; Schema: helpdesk; Owner: -
--

CREATE SEQUENCE helpdesk.request_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: request_type_id_seq; Type: SEQUENCE OWNED BY; Schema: helpdesk; Owner: -
--

ALTER SEQUENCE helpdesk.request_type_id_seq OWNED BY helpdesk.request_type.id;


--
-- Name: setting; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.setting (
    name text DEFAULT 'default'::text NOT NULL,
    default_status_id bigint NOT NULL,
    assigned_status_id bigint NOT NULL,
    default_department_id bigint NOT NULL,
    system_user_id bigint NOT NULL
);


--
-- Name: ticket; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.ticket (
    id bigint NOT NULL,
    request_type_id bigint NOT NULL,
    requester_id bigint NOT NULL,
    assigned_to_id bigint,
    department_id bigint NOT NULL,
    priority_id bigint NOT NULL,
    status_id bigint CONSTRAINT ticket_status_id_not_null1 NOT NULL,
    description text NOT NULL,
    due_date timestamp with time zone,
    created_at timestamp with time zone DEFAULT now() NOT NULL,
    updated_at timestamp with time zone DEFAULT now() NOT NULL
);


--
-- Name: ticket_activity; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.ticket_activity (
    id bigint NOT NULL,
    ticket_id bigint NOT NULL,
    profile_id bigint NOT NULL,
    kind helpdesk.ticket_activity_kind NOT NULL,
    body text DEFAULT ''::text NOT NULL,
    metadata jsonb DEFAULT '{}'::jsonb NOT NULL,
    created_at timestamp with time zone DEFAULT now() NOT NULL
);


--
-- Name: ticket_activity_id_seq; Type: SEQUENCE; Schema: helpdesk; Owner: -
--

CREATE SEQUENCE helpdesk.ticket_activity_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: ticket_activity_id_seq; Type: SEQUENCE OWNED BY; Schema: helpdesk; Owner: -
--

ALTER SEQUENCE helpdesk.ticket_activity_id_seq OWNED BY helpdesk.ticket_activity.id;


--
-- Name: ticket_id_seq; Type: SEQUENCE; Schema: helpdesk; Owner: -
--

CREATE SEQUENCE helpdesk.ticket_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: ticket_id_seq; Type: SEQUENCE OWNED BY; Schema: helpdesk; Owner: -
--

ALTER SEQUENCE helpdesk.ticket_id_seq OWNED BY helpdesk.ticket.id;


--
-- Name: ticket_status; Type: TABLE; Schema: helpdesk; Owner: -
--

CREATE TABLE helpdesk.ticket_status (
    id bigint NOT NULL,
    name text NOT NULL,
    display_name text NOT NULL,
    trait helpdesk.ticket_trait NOT NULL
);


--
-- Name: ticket_status_id_seq; Type: SEQUENCE; Schema: helpdesk; Owner: -
--

CREATE SEQUENCE helpdesk.ticket_status_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: ticket_status_id_seq; Type: SEQUENCE OWNED BY; Schema: helpdesk; Owner: -
--

ALTER SEQUENCE helpdesk.ticket_status_id_seq OWNED BY helpdesk.ticket_status.id;


--
-- Name: schema_migrations; Type: TABLE; Schema: public; Owner: -
--

CREATE TABLE public.schema_migrations (
    version character varying(255) NOT NULL
);


--
-- Name: session id; Type: DEFAULT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.session ALTER COLUMN id SET DEFAULT nextval('auth.session_id_seq'::regclass);


--
-- Name: user id; Type: DEFAULT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth."user" ALTER COLUMN id SET DEFAULT nextval('auth.user_id_seq'::regclass);


--
-- Name: useroauth id; Type: DEFAULT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.useroauth ALTER COLUMN id SET DEFAULT nextval('auth.useroauth_id_seq'::regclass);


--
-- Name: department id; Type: DEFAULT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.department ALTER COLUMN id SET DEFAULT nextval('helpdesk.department_id_seq'::regclass);


--
-- Name: priority id; Type: DEFAULT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.priority ALTER COLUMN id SET DEFAULT nextval('helpdesk.priority_id_seq'::regclass);


--
-- Name: profile id; Type: DEFAULT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.profile ALTER COLUMN id SET DEFAULT nextval('helpdesk.profile_id_seq'::regclass);


--
-- Name: request_category id; Type: DEFAULT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.request_category ALTER COLUMN id SET DEFAULT nextval('helpdesk.request_category_id_seq'::regclass);


--
-- Name: request_type id; Type: DEFAULT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.request_type ALTER COLUMN id SET DEFAULT nextval('helpdesk.request_type_id_seq'::regclass);


--
-- Name: ticket id; Type: DEFAULT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket ALTER COLUMN id SET DEFAULT nextval('helpdesk.ticket_id_seq'::regclass);


--
-- Name: ticket_activity id; Type: DEFAULT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket_activity ALTER COLUMN id SET DEFAULT nextval('helpdesk.ticket_activity_id_seq'::regclass);


--
-- Name: ticket_status id; Type: DEFAULT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket_status ALTER COLUMN id SET DEFAULT nextval('helpdesk.ticket_status_id_seq'::regclass);


--
-- Name: session session_pkey; Type: CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.session
    ADD CONSTRAINT session_pkey PRIMARY KEY (id);


--
-- Name: session session_token_key; Type: CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.session
    ADD CONSTRAINT session_token_key UNIQUE (token);


--
-- Name: user user_pkey; Type: CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth."user"
    ADD CONSTRAINT user_pkey PRIMARY KEY (id);


--
-- Name: user user_username_key; Type: CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth."user"
    ADD CONSTRAINT user_username_key UNIQUE (username);


--
-- Name: useroauth useroauth_pkey; Type: CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.useroauth
    ADD CONSTRAINT useroauth_pkey PRIMARY KEY (id);


--
-- Name: useroauth useroauth_provider_id_key; Type: CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.useroauth
    ADD CONSTRAINT useroauth_provider_id_key UNIQUE (provider_id);


--
-- Name: useroauth useroauth_user_id_key; Type: CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.useroauth
    ADD CONSTRAINT useroauth_user_id_key UNIQUE (user_id);


--
-- Name: department department_name_key; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.department
    ADD CONSTRAINT department_name_key UNIQUE (name);


--
-- Name: department department_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.department
    ADD CONSTRAINT department_pkey PRIMARY KEY (id);


--
-- Name: priority priority_name_key; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.priority
    ADD CONSTRAINT priority_name_key UNIQUE (name);


--
-- Name: priority priority_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.priority
    ADD CONSTRAINT priority_pkey PRIMARY KEY (id);


--
-- Name: profile profile_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.profile
    ADD CONSTRAINT profile_pkey PRIMARY KEY (id);


--
-- Name: profile profile_user_id_key; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.profile
    ADD CONSTRAINT profile_user_id_key UNIQUE (user_id);


--
-- Name: request_category request_category_name_key; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.request_category
    ADD CONSTRAINT request_category_name_key UNIQUE (name);


--
-- Name: request_category request_category_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.request_category
    ADD CONSTRAINT request_category_pkey PRIMARY KEY (id);


--
-- Name: request_type request_type_name_key; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.request_type
    ADD CONSTRAINT request_type_name_key UNIQUE (name);


--
-- Name: request_type request_type_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.request_type
    ADD CONSTRAINT request_type_pkey PRIMARY KEY (id);


--
-- Name: setting setting_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.setting
    ADD CONSTRAINT setting_pkey PRIMARY KEY (name);


--
-- Name: ticket_activity ticket_activity_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket_activity
    ADD CONSTRAINT ticket_activity_pkey PRIMARY KEY (id);


--
-- Name: ticket ticket_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket
    ADD CONSTRAINT ticket_pkey PRIMARY KEY (id);


--
-- Name: ticket_status ticket_status_name_key; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket_status
    ADD CONSTRAINT ticket_status_name_key UNIQUE (name);


--
-- Name: ticket_status ticket_status_pkey; Type: CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket_status
    ADD CONSTRAINT ticket_status_pkey PRIMARY KEY (id);


--
-- Name: schema_migrations schema_migrations_pkey; Type: CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY public.schema_migrations
    ADD CONSTRAINT schema_migrations_pkey PRIMARY KEY (version);


--
-- Name: ticket_activity_ticket_created_at_idx; Type: INDEX; Schema: helpdesk; Owner: -
--

CREATE INDEX ticket_activity_ticket_created_at_idx ON helpdesk.ticket_activity USING btree (ticket_id, created_at);


--
-- Name: ticket set_ticket_updated_at; Type: TRIGGER; Schema: helpdesk; Owner: -
--

CREATE TRIGGER set_ticket_updated_at BEFORE UPDATE ON helpdesk.ticket FOR EACH ROW EXECUTE FUNCTION common.set_updated_at();


--
-- Name: session session_user_id_fkey; Type: FK CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.session
    ADD CONSTRAINT session_user_id_fkey FOREIGN KEY (user_id) REFERENCES auth."user"(id);


--
-- Name: useroauth useroauth_user_id_fkey; Type: FK CONSTRAINT; Schema: auth; Owner: -
--

ALTER TABLE ONLY auth.useroauth
    ADD CONSTRAINT useroauth_user_id_fkey FOREIGN KEY (user_id) REFERENCES auth."user"(id);


--
-- Name: profile profile_department_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.profile
    ADD CONSTRAINT profile_department_id_fkey FOREIGN KEY (department_id) REFERENCES helpdesk.department(id);


--
-- Name: profile profile_user_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.profile
    ADD CONSTRAINT profile_user_id_fkey FOREIGN KEY (user_id) REFERENCES auth."user"(id);


--
-- Name: request_type request_type_category_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.request_type
    ADD CONSTRAINT request_type_category_id_fkey FOREIGN KEY (category_id) REFERENCES helpdesk.request_category(id);


--
-- Name: request_type request_type_default_priority_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.request_type
    ADD CONSTRAINT request_type_default_priority_id_fkey FOREIGN KEY (default_priority_id) REFERENCES helpdesk.priority(id);


--
-- Name: setting setting_assigned_status_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.setting
    ADD CONSTRAINT setting_assigned_status_id_fkey FOREIGN KEY (assigned_status_id) REFERENCES helpdesk.ticket_status(id);


--
-- Name: setting setting_default_department_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.setting
    ADD CONSTRAINT setting_default_department_id_fkey FOREIGN KEY (default_department_id) REFERENCES helpdesk.department(id);


--
-- Name: setting setting_default_status_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.setting
    ADD CONSTRAINT setting_default_status_id_fkey FOREIGN KEY (default_status_id) REFERENCES helpdesk.ticket_status(id);


--
-- Name: setting setting_system_user_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.setting
    ADD CONSTRAINT setting_system_user_id_fkey FOREIGN KEY (system_user_id) REFERENCES helpdesk.profile(id);


--
-- Name: ticket_activity ticket_activity_profile_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket_activity
    ADD CONSTRAINT ticket_activity_profile_id_fkey FOREIGN KEY (profile_id) REFERENCES helpdesk.profile(id);


--
-- Name: ticket_activity ticket_activity_ticket_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket_activity
    ADD CONSTRAINT ticket_activity_ticket_id_fkey FOREIGN KEY (ticket_id) REFERENCES helpdesk.ticket(id);


--
-- Name: ticket ticket_assigned_to_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket
    ADD CONSTRAINT ticket_assigned_to_id_fkey FOREIGN KEY (assigned_to_id) REFERENCES helpdesk.profile(id);


--
-- Name: ticket ticket_department_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket
    ADD CONSTRAINT ticket_department_id_fkey FOREIGN KEY (department_id) REFERENCES helpdesk.department(id);


--
-- Name: ticket ticket_priority_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket
    ADD CONSTRAINT ticket_priority_id_fkey FOREIGN KEY (priority_id) REFERENCES helpdesk.priority(id);


--
-- Name: ticket ticket_request_type_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket
    ADD CONSTRAINT ticket_request_type_id_fkey FOREIGN KEY (request_type_id) REFERENCES helpdesk.request_type(id);


--
-- Name: ticket ticket_requester_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket
    ADD CONSTRAINT ticket_requester_id_fkey FOREIGN KEY (requester_id) REFERENCES helpdesk.profile(id);


--
-- Name: ticket ticket_status_id_fkey; Type: FK CONSTRAINT; Schema: helpdesk; Owner: -
--

ALTER TABLE ONLY helpdesk.ticket
    ADD CONSTRAINT ticket_status_id_fkey FOREIGN KEY (status_id) REFERENCES helpdesk.ticket_status(id);


--
-- PostgreSQL database dump complete
--

\unrestrict dbmate


--
-- Dbmate schema migrations
--

INSERT INTO public.schema_migrations (version) VALUES
    ('20260425223043');

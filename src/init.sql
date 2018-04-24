CREATE SCHEMA wkd;
CREATE ROLE wkd_notify_user LOGIN PASSWORD 'wkd_notify_password';

CREATE SEQUENCE wkd.sdevice
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 1
  CACHE 1
  NO CYCLE;

ALTER SEQUENCE wkd.sdevice OWNER TO postgres;

-- ALTER SEQUENCE wkd.sdevice RESTART;

GRANT SELECT,UPDATE,USAGE ON TABLE wkd.sdevice TO postgres;
GRANT SELECT,UPDATE,USAGE ON TABLE wkd.sdevice TO wkd_notify_user;

CREATE TABLE wkd.rdevice (
  id integer NOT NULL DEFAULT nextval(('wkd.sdevice'::text)::regclass),
  profile_id integer NOT NULL,
  apns_device_id character varying(255),
  CONSTRAINT rdevice_pk PRIMARY KEY (id)
) WITH (
  OIDS=FALSE
);

ALTER TABLE wkd.rdevice OWNER TO postgres;

GRANT SELECT,UPDATE,INSERT,DELETE,TRUNCATE,REFERENCES,TRIGGER ON TABLE wkd.rdevice TO postgres;
GRANT SELECT,INSERT,DELETE ON TABLE wkd.rdevice TO wkd_notify_user;

CREATE SEQUENCE wkd.snotification
  INCREMENT 1
  MINVALUE 1
  MAXVALUE 9223372036854775807
  START 1
  CACHE 1
  NO CYCLE;

ALTER SEQUENCE wkd.snotification OWNER TO postgres;

-- ALTER SEQUENCE wkd.snotification RESTART;

GRANT SELECT,UPDATE,USAGE ON TABLE wkd.snotification TO postgres;
GRANT SELECT,UPDATE,USAGE ON TABLE wkd.snotification TO wkd_notify_user;

CREATE OR REPLACE FUNCTION wkd.notification_trg()
  RETURNS trigger AS
$BODY$
DECLARE
    
BEGIN
    NEW.id = COALESCE(NEW.id, nextval('wkd.snotification'::text::regclass));
    IF TG_OP = 'INSERT' THEN
        PERFORM pg_notify('wkd_notify', 'NEW ' ||- NEW.id::text);
    ELSIF TG_OP = 'UPDATE' AND NEW.rread IS DISTINCT FROM OLD.rread THEN
        PERFORM pg_notify('wkd_notify', 'READ ' ||- NEW.id::text);
    END IF;
    
    RETURN NEW;
END;
$BODY$
  LANGUAGE plpgsql VOLATILE
  COST 100;

ALTER FUNCTION wkd.notification_trg() OWNER TO postgres;
GRANT EXECUTE ON FUNCTION wkd.notification_trg() TO postgres;
GRANT EXECUTE ON FUNCTION wkd.notification_trg() TO public;

CREATE TABLE wkd.rnotification (
  id integer NOT NULL DEFAULT nextval(('wkd.snotification'::text)::regclass),
  recipient_id integer NOT NULL,
  title text,
  body text,
  extra json,
  rread timestamp without time zone,
  create_stamp timestamp without time zone DEFAULT current_timestamp,
  CONSTRAINT rnotification_pk PRIMARY KEY (id)
) WITH (
  OIDS=FALSE
);

ALTER TABLE wkd.rnotification OWNER TO postgres;

-- Trigger: rnotification_notify_trg ON wkd.rnotification;
-- DROP TRIGGER rnotification_notify_trg ON wkd.rnotification;
CREATE TRIGGER rnotification_notify_trg
   BEFORE INSERT OR UPDATE
   ON wkd.rnotification
   FOR EACH ROW
   EXECUTE PROCEDURE wkd.notification_trg();



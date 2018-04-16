wickednotify(1) -- A Wicked Fast Notification Provider
======================================================

## FULL PATH
This instance of WickedNotify was installed as `@prefix@/sbin/wickednotify` (Usually `/usr/local/sbin/wickednotify`)

## SYNOPSIS
```
wickednotify
       [-h                             | --help]
       [-v                             | --version]
       [-c <config-file>               | --config-file=<config-file>]
       [-p <port>                      | ---port=<port>]
       [-l <log-level>                 | --log-level=<log-level>]
       [-d <device-table>              | --device-table=<device-table>]
       [-n <notification-table>        | --notification-table=<notification-table>]
       [-a <account-table>             | --account-table=<account-table>]
       [-k <account-table-pk-column>   | --account-table-pk-column=<account-table-pk-column>]
       [-g <account-table-guid-column> | --account-table-guid-column=<account-table-guid-column>]
       [-N <listen-channel>            | --listen-channel=<listen-channel>]
       [-U <pg-user>                   | --pg-user=<pg-user>]
       [-W <pg-password>               | --pg-password=<pg-password>]
       [-H <pg-host>                   | --pg-host=<pg-host>]
       [-P <pg-port>                   | --pg-port=<pg-port>]
       [-D <pg-database>               | --pg-database=<pg-database>]
       [-S <pg-sslmode>                | --pg-sslmode=<pg-sslmode>]
       [-M <apns-mode>                 | --apns-mode=<apns-mode>]
       [-C <apns-cert>                 | --apns-cert=<apns-cert>]
```

## DESCRIPTION
The `wickednotify` server is a notification provider that uses APNS to push to Apple devices, and a websocket server for other devices (including Android).

All log output is pushed to stderr, if you are pushing that to a file, then you must handle rotating the file yourself or it will get large and slow `wickednotify` down.


## OPTIONS
`-h` or `--help`  
       Print usage and exit

`-v` or `--version`  
       Print version information and exit

`-c` or `--config-file=`  
       `String;` defaults to @prefix@/etc/wickednotify/wickednotify.conf  
       You can use this option to tell WickedNotify where to look for the configuration file. A sample configuration file is provided in @prefix@/etc/wickednotify. If there is no file specified WickedNotify will look in the current directory for a config file. If no config file is found WickedNotify will proceed with default values.

The following options can be specified on the command line or in the configuration file. In the event a value is specified on the command line and in the config file, WickedNotify will always use the command line option. Note that if no option is specified then some options will be set to a default value.

`[command line short]` or `[command line long]` or `[config file]`

`-p` or `--port` or `port`  
       `Integer;` defaults to `8989`  
       Tells WickedNotify what port to listen for WebSocket request on.  

`-l` or `--log-level` or `log_level`  
       `String;` defaults to `error`  
       Sets the level of logs to output.  

`-d` or `--device-table` or `device_table`  
       `String;` defaults to `wkd.rdevice`  
`-n` or `--notification-table` or `notification_table`    
       `String;` defaults to `wkd.rnotification`  
`-a` or `--account-table` or `account_table`    
       `String;` defaults to `wkd.rprofile`  
`-k` or `--account-table-pk-column` or `account_table_pk_column`    
       `String;` defaults to `id`  
`-g` or `--account-table-guid-column` or `account_table_guid_column`    
       `String;` defaults to `guid`  
       These options allow you to change what tables/columns WickedNotify uses, so you can have multiple instances in the same database.` It is recommended to only change the `account-table` options to us`e your account table.  

`-N` or `--listen-channel` or `listen_channel`  
       `String;` defaults to `wkd_notify`  
       This option allows you to specify what channel WickedNotify LISTENs on. Must be a valid PostgreSQL identifier.  

`-U` or `--pg-user` or `pg_user`  
       `String;` defaults to `wkd_notify_user`  
`-W` or `--pg-password` or `pg_password`    
       `String;` defaults to `wkd_notify_password`  
`-H` or `--pg-host` or `pg_host`    
       `String;` defaults to `localhost`  
`-P` or `--pg-port` or `pg_port`    
       `String;` defaults to `5432`  
`-D` or `--pg-database` or `pg_database`    
       `String;` defaults to `postgres`  
`-S` or `--pg-sslmode` or `pg_sslmode`    
       `String;` defaults to `require`  
       These options allow you to connect to an external PostgreSQL server.

`-M` or `--apns-mode` or `apns_mode`  
       `String;` defaults to `development`  
       The APNS mode can be development or production. Note that development APNS ids do not work in the production mode, and vice versa.  

`-C` or `--apns-cert` or `apns_cert`  
       `String;` no default  
       Required, this must be in a pkcs12 format. To get the pkcs12 certificate, you must open the certificate you downloaded from Apple in Keychain Access, and export it as a .p12 file.


## EXAMPLES
Run `wickednotify` (short argument):
```
@prefix@/sbin/wickednotify -c @prefix@/etc/wickednotify/wickednotify.conf
```

Run `wickednotify` (long argument):
```
@prefix@/sbin/wickednotify --config-file=@prefix@/etc/wickednotify/wickednotify.conf
```

## TROUBLESHOOTING
Can't connect to PostgreSQL:
       Try accessing your database through psql. If you can, double check your connection string parameters. If you can't connect, you may have a firewall problem.
Can't connect to APNS:
       Try to ping the APNS service: api.push.apple.com


## AUTHOR
Copyright (c) 2018 Annunziato Tocci

Created by Annunziato Tocci

Report bugs to https://github.com/wickednotify/wickednotify-server/issues  
Send us feedback! nunzio@tocci.xyz

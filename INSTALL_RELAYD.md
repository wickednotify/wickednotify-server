# Installing WickedNotify behind a relayd reverse proxy on OpenBSD

*Note: Assumes running WickedNotify at `127.0.0.1:8989`, adjust your values accordingly.*

`/etc/relayd.conf`:
```
table <wickednotify> { 127.0.0.1 }

http protocol "protowickednotify" {
        match request path "/notifications" quick header "Host" value "192.168.44.111" \
                forward to <wickednotify>

        tls { ciphers "MEDIUM:HIGH" }
}

relay "wickednotify" {
        listen on 0.0.0.0 port 443 tls
        protocol "protowickednotify"

        forward to <wickednotify> port 8989
}

```

### SSL Certificates

Relayd assumes that the ssl files are:
- `/etc/ssl/<address>.crt` or `/etc/ssl/<address>:<port>.crt`
- `/etc/ssl/private/<address>.key` or `/etc/ssl/private/<address>:<port>.key`

With `<address>` being `0.0.0.0` and `<port>` being `443` from the `listen` directive.

Relayd also requires RSA certificates ([ssl(8)](https://man.openbsd.org/ssl.8)).

To self sign a certificate, execute these commands:
```
# openssl genrsa -out /etc/ssl/private/0.0.0.0.key 2048
# openssl req -new -key /etc/ssl/private/0.0.0.0.key \ 
  -out /etc/ssl/private/0.0.0.0.csr
# openssl x509 -sha256 -req -days 365 \ 
  -in /etc/ssl/private/0.0.0.0.csr \ 
  -signkey /etc/ssl/private/0.0.0.0.key \ 
  -out /etc/ssl/0.0.0.0.crt
```

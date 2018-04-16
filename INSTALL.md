# Installing WickedNotify

## Dependencies

### LIBPQ
In order for WickedNotify to talk to PostgreSQL you need to have the libpq library installed. If you don't have LibPQ or the WickedNotify compile process can't find it, please consult the file INSTALL_LIBPQ for some OS-specific advice on how to get libpq.

### SSL
WickedNotify does not listen on https, but it does need SSL to connect to APNS.

WickedNotify works with OpenSSL or LibreSSL, if you wish to use OpenSSL:
```
sudo apt install libssl-dev # Ubuntu
sudo dnf install openssl-devel # Fedora
sudo yum install openssl-devel # CentOS/RHEL
brew install openssl # macOS *
```
Or if you choose LibreSSL, make sure its `openssl` is first in the $PATH (On OpenBSD you don't need to worry about this).

*\* Apple does not include a good enough version of OpenSSL. We officially support the `brew` versions of OpenSSL. If installing OpenSSL from source (untested) make sure to install static libraries.*

### NGHTTP2
WickedNotify requires nghttp2 libraries to connect to APNS.

```
sudo apt install libnghttp2-dev # Ubuntu
sudo dnf install libnghttp2-devel # Fedora
sudo yum install libnghttp2-devel # CentOS/RHEL
brew install nghttp2 # macOS *
```

#### Downloading WickedNotify

https://github.com/wickednotify/wickednotify-server/releases

#### Installing WickedNotify

If you'd like to test WickedNotify before you install, see the section "Testing WickedNotify Before Installing" further down.
```
cd wickednotify
./configure
make
sudo make install
```
If you are on OpenBSD or FreeBSD, use gmake instead.
WickedNotify will be installed in `/usr/local/sbin`. All other files such as the html, javascript and configuration files will be installed to `/usr/local/etc/wickednotify`.

#### Running WickedNotify

To run WickedNotify:
```
/usr/local/sbin/wickednotify
```
Long Version:
```
/usr/local/sbin/wickednotify \
-c /usr/local/etc/wickednotify/wickednotify.conf
```
#### Configuring WickedNotify

Before running WickedNotify for the first time you must configure the APNS certificate. All the options are explained in the WickedNotify man file:
```
man wickednotify
```
Current configuration options allow you to set various database object names, the listen port and log level.

You'll also need to set the PostgreSQL conenction options to tell WickedNotify where your PostgreSQL databases are published.

There is an `init.sql` script in the `src` directory that contains the PostgreSQL schema required for WickedNotify's operation.

*Note: you need to create your own account table, with an integer PK and some form of text-based GUID column (not the id, that's too easy to guess) and configure WickedNotify accordingly.*


#### Reverse proxy for SSL
For nginx, see [INSTALL_NGINX.md](https://github.com/wickednotify/wickednotify-server/blob/master/INSTALL_NGINX.md)<br />
For OpenBSD's relayd, see [INSTALL_RELAYD.md](https://github.com/wickednotify/wickednotify-server/blob/master/INSTALL_RELAYD.md)

#### Testing WickedNotify Before Installing
```
cd wickednotify
./configure
make
nano src/config/wickednotify.conf
make test
```
If you want to test WickedNotify before you install, edit the `config/wickednotify.conf` file to specify your Postgres database and APNS certificate. To look at the WickedNotify man page before installing WickedNotify:
```
./configure
man -M man wickednotify
```
By default WickedNotify runs on port 8989, so if you need to change that you do it in the `wickednotify.conf` file.

Once you've set up your configuration, start the WickedNotify server with:
```
make test
```
WickedNotify will push a message like:
```
WickedNotify is Started
```
Once you see that message that means WickedNotify is running, download one of the samples and run it.

#### Uninstalling WickedNotify

If you still have your original build directory then:
```
cd wickednotify
./configure
make uninstall
```

Or, if you don't have your original build directory check the following locations:
```
rm -r /usr/local/etc/wickednotify        # you may wish to save your config file first
rm /usr/local/sbin/wickednotify          # this removes the binary
rm /usr/local/man/man1/wickednotify.1    # this removes the man page
```

# test-live555

## Compilation with live555 from repository

```
apt install git gcc liblivemedia-dev pkg-config
git clone https://github.com/Jet1oeil/test-live555.git
cd test-live555
make
./TestLiveMedia --username admin --password password --tcp -vvv rtsp://192.168.5.60/onvif/profile2/media.smp
```

## Compilation with live555 from sources

Optional complication of LiveMedia (Use the MakeFile.custom)
(Useful for Debian Bullseye)

Livemedia compilation :

```
apt install git gcc pkg-config libssl-dev gcc wget
wget http://www.live555.com/liveMedia/public/live555-latest.tar.gz
tar xvzf live555-latest.tar.gz
cd live
./genMakefiles linux
make
mkdir /opt/livemedia/
chmod a+w /opt/livemedia
make install PREFIX=/opt/livemedia/
```

Program compilation :

```
apt install git gcc liblivemedia-dev pkg-config
git clone https://github.com/Jet1oeil/test-live555.git
cd test-live555
make -f Makefile.custom
./TestLiveMedia --username admin --password password --tcp -vvv rtsp://192.168.5.60/onvif/profile2/media.smp
```

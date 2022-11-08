# test-live555

```
apt install git gcc liblivemedia-dev pkg-config
git clone https://github.com/Jet1oeil/test-live555.git
cd test-live555
make
./TestLiveMedia --username admin --password password --tcp -vvv rtsp://192.168.5.60/onvif/profile2/media.smp
```

Optional complication of LiveMedia (Use the MakeFile.custom)
(Useful for Debian Bullseye)


```
apt install libssl-dev gcc wget
wget http://www.live555.com/liveMedia/public/live555-latest.tar.gz
tar xvzf live555-latest.tar.gz 
cd live
./genMakefiles linux
make
```


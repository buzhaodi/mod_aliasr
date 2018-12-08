BASE=../../../..
LOCAL_LDFLAGS = -Llinux -lnlsCppSdk -lnlsCommonSdk -lcurl -lssl -lcrypto -lopus -lpthread -luuid -ljsoncpp
include $(BASE)/build/modmake.rules

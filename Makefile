CC:=gcc

SERVER = server
CLIENT = client
UNICAST = unicast

all: 
	$(CC) -o $(SERVER) multicast_$(SERVER).c -lliquid
	$(CC) -o $(CLIENT) multicast_$(CLIENT).c -lliquid
	$(CC) -o $(UNICAST) $(UNICAST).c -lliquid
	$(CC) -o $(CLIENT)_2/$(CLIENT)2 multicast_$(CLIENT).c -lliquid
	$(CC) -o $(CLIENT)_3/$(CLIENT)3 multicast_$(CLIENT).c -lliquid

clean:
	rm -rf $(SERVER) $(CLIENT) $(UNICAST)
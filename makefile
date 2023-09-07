CC = g++
STD = -std=c++17
OPT = -g
TARGET = server
OTHERS_TARGET = ./base/net.o ./base/http.o
LIB = -lpthread

default:$(TARGET)

%.o: %.h %.cpp
	$(CC) $(STD) $(OPT) -c $*.cpp -o $*.o

$(TARGET): $(TARGET).o $(OTHERS_TARGET)
	$(CC) $(STD) $(OPT) $(TARGET).o $(OTHERS_TARGET) -o $(TARGET) $(LIB)

$(TARGET).o: $(TARGET).cpp
	$(CC) $(STD) $(OPT) -c $(TARGET).cpp -o $(TARGET).o

clean:
	rm ./base/*.o
	rm *.o
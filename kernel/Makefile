TARGET = kernel.elf
OBJS = kernel.o

CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++11
LDFLAGS += --entry KernelMain -z norelro --image-base 0x100000 -z separate-code --static

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o

$(TARGET): $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $<

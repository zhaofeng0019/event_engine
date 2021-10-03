obj = mount_info.o comm_func.o perf_sample_record.o util.o trace_event_field.o  ring_buffer.o raw_data.o monitor.o example.o

cc = g++

flag = -g

build: $(obj)

$(obj) : %.o : %.cpp
	$(cc) -c -fPIC $(flag) $< -o $@

example: build 
	$(cc) $(flag) -static -o a.out  $(obj) -Wl,--whole-archive -lrt -lpthread -Wl,--no-whole-archive

clean:
	rm -rf *.o *.out obj/*.o

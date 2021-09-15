obj = mount_info.o comm_func.o perf_sample_record.o util.o trace_event_field.o  ring_buffer.o raw_data.o

cc = g++

flag = -g

build: $(obj)

$(obj) : %.o : %.cpp
	$(cc) -c $(flag) $< -o $@

clean:
	rm -rf *.o *.out
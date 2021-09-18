obj = mount_info.o comm_func.o perf_sample_record.o util.o trace_event_field.o  ring_buffer.o raw_data.o monitor.o

cc = g++

flag = -g

build: $(obj)
	$(cc) $(flag) -o a.out $(obj) -lpthread


$(obj) : %.o : %.cpp
	$(cc) -c $(flag) $< -o $@

clean:
	rm -rf *.o *.out obj/*.o
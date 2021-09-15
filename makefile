obj =mount_info.o comm_func.o perf_sample_record.o util.o trace_event_field.o  ring_buffer.o

cc = g++

mount_info.o : mount_info.cpp mount_info.h
	$(cc) -c mount_info.cpp

perf_sample_record.o : perf_sample_record.cpp perf_sample_record.h util.h
	$(cc) -c perf_sample_record.cpp

util.o : util.cpp util.h
	$(cc) -c util.cpp

trace_event_field.o : trace_event_field.cpp trace_event_field.h util.h
	$(cc) -c trace_event_field.cpp

ring_buffer.o : ring_buffer.cpp ring_buffer.h
	$(cc) -c ring_buffer.cpp

comm_func.o : comm_func.cpp comm_func.h mount_info.h trace_event_field.h
	$(cc) -c comm_func.cpp


build:$(obj)
	
clean:
	rm -rf *.o *.out
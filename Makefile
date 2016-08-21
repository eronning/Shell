CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=c99 -Werror

PROMPT = -DPROMPT
SH_OBJ = sh.c
SH = 33sh
SHNO = 33noprompt

all: 33sh 33noprompt

33noprompt: $(SH_OBJ)
	$(CC) $(CFLAGS) -o $(SHNO) $(SH_OBJ)

33sh: $(SH_OBJ)
	$(CC) $(CFLAGS) -o $(SH) $(PROMPT) $(SH_OBJ)

clean:
	rm -f $(SH) $(SHNO)


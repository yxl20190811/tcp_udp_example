# === Configuration ===
CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -std=c99 -O2
CPPFLAGS ?=
LDFLAGS  ?=
LIBS     ?= -lpthread

# === Directories ===
SRCDIR    := src
BINDIR    := bin
OBJDIR    := .obj

# === Targets (executables) ===
TARGETS   := $(BINDIR)/udp_server $(BINDIR)/tcp_server $(BINDIR)/test_client

# === Source files ===
UDP_SERVER_SRC    := $(SRCDIR)/udp_server.c
TCP_SERVER_SRC    := $(SRCDIR)/tcp_server.c
TEST_CLIENT_SRC   := $(SRCDIR)/test_client.c
SEND_ALL_SRC      := $(SRCDIR)/send_all.c

# === Object files ===
UDP_SERVER_OBJ    := $(OBJDIR)/udp_server.o
TCP_SERVER_OBJ    := $(OBJDIR)/tcp_server.o
TEST_CLIENT_OBJ   := $(OBJDIR)/test_client.o
SEND_ALL_OBJ      := $(OBJDIR)/send_all.o

# === Dependency files ===
DEPS := $(UDP_SERVER_OBJ:.o=.d) $(TCP_SERVER_OBJ:.o=.d) $(TEST_CLIENT_OBJ:.o=.d) $(SEND_ALL_OBJ:.o=.d)

# === Default target ===
.PHONY: all clean help

all: $(TARGETS)

# === Build each executable ===
$(BINDIR)/udp_server: $(UDP_SERVER_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@

$(BINDIR)/tcp_server: $(TCP_SERVER_OBJ) $(SEND_ALL_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

$(BINDIR)/test_client: $(TEST_CLIENT_OBJ) $(SEND_ALL_OBJ)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

# === Compile rule with dependency generation ===
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

# === Ensure object directory exists ===
$(OBJDIR):
	mkdir -p $@

# === Include auto-generated dependencies ===
-include $(DEPS)

# === Clean build artifacts ===
clean:
	rm -rf $(OBJDIR) $(BINDIR)/*.log $(BINDIR)/*.* $(BINDIR)/test $(BINDIR)/tcp $(BINDIR)/udp

# === Help target ===
help:
	@echo "Available targets:"
	@echo "  all          - Build all executables"
	@echo "  udp_server   - Build UDP logging server"
	@echo "  tcp_server   - Build TCP-to-UDP proxy server"
	@echo "  test_client  - Build test client"
	@echo "  clean        - Remove all build artifacts"
	@echo "  help         - Show this message"
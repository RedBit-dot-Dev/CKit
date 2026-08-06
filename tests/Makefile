CC = gcc

ifeq ($(OS),Windows_NT)
    # Windows settings
	CFLAGS = -std=c99 -Wpedantic -Wall -Wextra -Werror=cast-align=strict -Wlogical-op -Wunreachable-code -g -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -fanalyzer -O0
    RM = del
    FixPath = $(subst /,\,$1)
	FixExt = $(addsuffix .exe,$1)
	FixRun = $1
else
    # Unix settings
	CFLAGS = -std=c99 -Wpedantic -Wall -Wextra -Werror=cast-align=strict -Wlogical-op -Wunreachable-code -fsanitize=address,alignment,undefined -g -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -fanalyzer -O0
    RM = rm
    FixPath = $1
	FixExt = $1
	FixRun = $(addprefix ./,$1)
endif

INCLUDE_SRC = $(call FixPath,../)

test:
	$(CC) $(CFLAGS) -I$(INCLUDE_SRC) -c $(call FixPath,$(TARGET_DIR)/$(TARGET_NAME).c) -o $(call FixPath,$(TARGET_DIR)/$(TARGET_NAME).o)
	$(CC) $(CFLAGS) -I$(INCLUDE_SRC) $(call FixPath,$(TARGET_DIR)/$(TARGET_NAME).o) -o $(call FixPath,$(TARGET_DIR)/$(call FixExt,$(TARGET_NAME)))
	$(RM) $(call FixPath,$(TARGET_DIR)/$(TARGET_NAME).o)
	$(call FixRun,$(call FixPath,$(TARGET_DIR)/$(call FixExt,$(TARGET_NAME))))

clean:
	$(RM) $(call FixPath,$(TARGET_DIR)/$(call FixExt,$(TARGET_NAME)))

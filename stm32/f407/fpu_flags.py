Import("env")

# 硬件浮点参数必须同时传给编译、汇编和链接阶段，
# 否则链接时报 "uses VFP register arguments, firmware.elf does not"
fpu_flags = ["-mfpu=fpv4-sp-d16", "-mfloat-abi=hard"]
env.Append(CCFLAGS=fpu_flags, ASFLAGS=fpu_flags, LINKFLAGS=fpu_flags)

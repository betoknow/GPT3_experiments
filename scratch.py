
z = 237.0


for N in range(-10, 10):
    x = int(abs(z) * 2**N)
    print('xxx', x)
    if  x & 0x100:
        break
        
N = -N
if z <0:
    x = -x

v = ((N << 11) | (x & 0x3ff)) & 0xffff
print(hex(v))
print(bin(N)) 
print(bin(x), len(bin(x))-2)    
print(x*2**N)



def add(x,y):
    return x+y

def kernel(N : int,a,b,c):
    for i in range(N):
        c[i] = add(a[i],b[i])

max_iter = 256

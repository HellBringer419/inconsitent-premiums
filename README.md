# inconsistent-premiums
Uses the [Fyers SDK](https://github.com/FyersDev/fyers-c-sdk) to find inconsistent premiums of monthly expiry.

Plug in our Fyers App credentials and compile by linking the Fyers *.so libraries.
Also needs math.h & cjson.h.

## Sample output
<img width="830" height="768" alt="Screenshot showing sample output" src="https://github.com/user-attachments/assets/894e813a-99cf-4e9e-90fc-a845b84e8546" />

The first column shows the premium and the monthly instrument that we are dealing with (calling it the "leg").
Second column shows the difference between the leg's strike and the futures. And in brackets difference between legs's premium and Nifty Future.
The numbers in bracket is the interesting difference and this is what we we can pocketDirect leak of 99 byte(s) in 1 object(s) allocated from:. 

Third column is the same thing but with the Nifty LTP.
Fourth columns is the difference between the current leg and the one before it. Since they are moving 50 points, it's always 50.
But the number in brackets show the premium difference between current leg and the last one. This should ideally be close to 50 but when it's not, we can pocket the difference.

## WHY
This is a hobby project using my software developer skills to understand algo trading and enhance my skillset. 
Why C & not web-frameworks like Spring Boot & NodeJS? 
Algo trading needs to be fast. While you can get p95 down a few microseconds with Redis and optimized SQL, algo trading needs to do CPU intensive work where micro seconds is the norm and C being so slim can actually handle it. Non GC languages have the advantage in this space.
In the current program, we are as fast as the network call to Fyers can be. So pretty fast.
 

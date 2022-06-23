// A Lehmer (MLCG) RNG using Number for math.
//
// In order to accurately do integer math with 64-bit doubles, we have to
// make sure that all numbers involved are smaller than 2**53.
//
// Use parameters from Pierre l'Ecuyers Table 3 where a(m-1) < 2**53
//
// https://www.ams.org/journals/mcom/1999-68-225/S0025-5718-99-00996-5/S0025-5718-99-00996-5.pdf

const m = 2**35 - 31
const a = 185852

// Initialize the seed with the current time.
var seed = Date.now() % m

// Reset the sequence with a user-provided seed.
function srandom(s) {
	seed = s % m
}

// Return a random floating point number 0 < x < 1.
function random() {
	return (seed = seed * a % m) / m
}

// Return a random integer 0 <= x < k.
function randomInt(k) {
	return (seed = seed * a % m) % k
}

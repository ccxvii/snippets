// A Lehmer (MLCG) RNG using BigInt for math.
//
// In order to use the resulting state with double precision numbers, we have
// to pick an m-parameter less than 2**53.
//
// Use parameters from Pierre l'Ecuyers table.
//
// https://www.ams.org/journals/mcom/1999-68-225/S0025-5718-99-00996-5/S0025-5718-99-00996-5.pdf

const mm = 2**53 - 111
const m = BigInt(mm)
const a = 5667072534355537n

// Initialize the seed with the current time.
var seed = BigInt(Date.now()) % m

// Reset the sequence with a user-provided seed.
function srandom(s) {
	seed = BigInt(s) % m
}

// Return a random floating point number 0 < x < 1.
function random() {
	return Number(seed = seed * a % m) / mm
}

// Return a random integer 0 < x <= k.
function randomInt(k) {
	return Number(seed = seed * a % m) % k
}

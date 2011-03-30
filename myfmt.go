package main

import (
	"os";
	"fmt";
	"bufio";
	"strings";
	"regexp";
)

var input = bufio.NewReader(os.Stdin)
var braces = 0
var parens = 0
var comment = false
var backslash = false
var laststar = false
var statements = 0

var wordrx = regexp.MustCompile("(^|[^A-Za-z0-9_])(if|else|while|for|return)($|[^A-Za-z0-9_])")
var stringrx = regexp.MustCompile(`("([^"]|\\")*")|('\\?.')`);

func printIndent(count int) {
	if count < 0 {
		count = 0
	}
}

func removeComments(line string) string {
	var rem string;

	x := strings.Index(line, "//");
	if x > -1 {
		line = line[x:len(line)]
	}

	if !comment {
		x = strings.Index(line, "/*");
		if x > -1 {
			rem = line[x:];
			line = line[0:x];
			x = strings.Index(rem, "*/");
			if x > -1 {
				rem = rem[x+2:];
				line = strings.Join([]string{line, rem}, " ");
				comment = false;
			} else {
				comment = true
			}
		}
	} else {
		x = strings.Index(line, "*/");
		if x > -1 {
			line = line[x+2:];
			comment = false;
		} else {
			line = ""
		}
	}
	return line;
}

func formatLine(line string) {
	// leave macros alone
	if strings.HasSuffix(line, "\\\n") {
		backslash = true;
		fmt.Print(line);
		return
	}
	if backslash {
		backslash = false;
		fmt.Print(line);
		return
	}
	
	line = strings.TrimSpace(line);
	
	if strings.HasPrefix(line, "#") {
		fmt.Println(line);
		return
	}
	
	code := line;
	code = stringrx.ReplaceAllString(code, "\"X\"");
	code = removeComments(code);

	obraces := strings.Count(code, "{");
	cbraces := strings.Count(code, "}");
	oparens := strings.Count(code, "(");
	cparens := strings.Count(code, ")");
	
	depth := braces;
	if obraces != cbraces && strings.HasPrefix(code, "}") {
		depth -= cbraces
	}

	// indent after statement before block or ;
	if strings.Count(code, "{") > 0 || strings.Count(code, "}") > 0 {
		statements = 0
	}
	depth += statements;
	
	if wordrx.MatchString(code) {
		statements ++;
	}
	if strings.Count(code, "{") > 0 || strings.Count(code, "}") > 0 {
		statements = 0
	}
	if strings.HasSuffix(code, ";") {
		statements = 0
	}
	
	// inside a multiline parenthesis (function params, call, if case)
	if parens > 0 && statements == 0 {
		depth++
	}

	// case: default: and other labels
	if strings.HasPrefix(code, "case") && strings.Count(code, ":") > 0 {
		depth--
	} else if strings.HasPrefix(code, "default") && strings.Count(code, ":") > 0 {
		depth--
	} else if strings.HasSuffix(code, ":") {
		depth = 0
	}
	if depth < 0 {
		depth = 0
	}

	if len(line) > 0 {
		for i := 0; i < depth; i++ {
			fmt.Print("\t")
		}
	}

	// multi line star comment
	if laststar && strings.HasPrefix(line, "*/") {
		fmt.Print(" ")
	}
	if comment && len(line) > 0 && line[0] == '*' {
		laststar = true;
		fmt.Print(" ")
	} else {
		laststar = false
	}

	// print the line!
	fmt.Println(line);

	braces += obraces - cbraces;
	parens += oparens - cparens;
}

func main() {
	for {
		line, error := input.ReadString('\n');
		if error == os.EOF && len(line) == 0 {
			os.Exit(0)
		}
		formatLine(line);
		if error == os.EOF {
			os.Exit(0)
		}
		if error != nil {
			fmt.Fprintf(os.Stderr, "error: %s", error);
			os.Exit(1);
		}
	}
}

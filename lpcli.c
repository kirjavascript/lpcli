#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define LP_STATIC
#define LP_IMPLEMENTATION
#include "lp.h"

#include "lpcli.h"

#define ERRORS_X \
	X(NONE, "Not an error") \
	X(OPTION, "Unrecognized or incorrect options specified") \
	X(VALUE, "Cannot set %s value to %i") \
	X(PASSWORD, "Failed to read the password") \
	X(GENERATE, "LP_generate returned error code %i") \
	X(CLIPBOARD, "Cannot copy to clipboard") \


// ERR_XXX
#define X(A, B) ERR_##A,
enum {ERRORS_X};
#undef X

// "XXX\n"
#define X(A, B) B "\n",
static const char *errstr[] = {ERRORS_X};
#undef X

#undef ERRORS_X

int print_usage(void)
{
	fprintf(stderr,
	    "Usage: lpcli <site> [login] [options]" "\n"
	    "Options:" "\n"
	    "  --lowercase, -l     include lowercase characters" "\n"
	    "  --uppercase, -u     include uppercase characters" "\n"
	    "  --digits, -d        include digits" "\n"
	    "  --symbols, -s       include symbols" "\n"
	    "\n"
	    "  --length, -n        number of characters (default 16)" "\n"
	    "  --counter, -c       number to add to salt (default 1)" "\n"
	    "\n"
	    "  --print, -p         print instead of copying to clipboard." "\n"
#ifdef USE_XCLIP
	    "                      xclip is required to copy to clipboard." "\n"
#endif
	);
	fflush(stderr);
	return LPCLI_FAIL;
}

int print_error(const char *format, ...)
{
	fputs("Error: ", stderr);
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fflush(stderr);

	return LPCLI_FAIL;
}

void hmac_sha256(const char *key, const char *salt, uint8_t *output) {
    HMAC_SHA256_CTX hmac;
    hmac_sha256_init(&hmac, (uint8_t *)key, strlen(key));
    hmac_sha256_update(&hmac, (uint8_t *)salt, strlen(salt));
    hmac_sha256_final(&hmac, output);
}

const char* getIcon(long unsigned int offset) {
    const char* icons[] = {
        "🏷️", "❤️", "🛏️", "🏫", "🔌", "🚑", "🚌", "🚗", "✈️", "🚀", "🚢", "🚇",
        "🚚", "💴", "💶", "₿", "💵", "💷", "🗄️", "📊", "🛏️", "🍺", "🔔", "🔭",
        "🎂", "💣", "💼", "🐛", "📷", "🛒", "📜", "☕", "☁️", "☕", "💬", "🧊",
        "🍴", "🗄️", "💎", "❗", "👁️", "🚩", "🧪", "⚽", "🎮", "🎓"
    };
    unsigned long int index = offset % (sizeof(icons) / sizeof(icons[0]));
    return icons[index];
}


const char* getColor(long unsigned int offset) {
    const char* terminal_colors[] = {
        "\033[97m", // Black (#000000)
        "\033[36m", // Cyan (#074750, #009191)
        "\033[36m", // Cyan (#009191)
        "\033[35m", // Magenta (#FF6CB6)
        "\033[35m", // Magenta (#FFB5DA)
        "\033[35m", // Magenta (#490092)
        "\033[34m", // Blue (#006CDB)
        "\033[35m", // Magenta (#B66DFF)
        "\033[34m", // Blue (#6DB5FE)
        "\033[34m", // Blue (#B5DAFE)
        "\033[31m", // Red (#920000)
        "\033[33m", // Yellow (#924900)
        "\033[33m", // Yellow (#DB6D00)
        "\033[32m", // Green (#24FE23)
    };

    unsigned int index = offset % (sizeof(terminal_colors) / sizeof(terminal_colors[0]));
    return terminal_colors[index];
}

//#define PRINT_ERROR(X,...) print_error(errstr[ERR_##A],__VA_ARGS__)

typedef struct parsed_args
{
	const char *site;
	const char *login;
	const char *password;
	unsigned charsets;
	unsigned length;
	unsigned counter;
	unsigned flags;
} LPCLI_OPTS;

//option flags
enum
{
	OPTS_CHARSETS = 0x01,
	OPTS_LENGTH   = 0x02,
	OPTS_COUNTER  = 0x04,
	OPTS_PRINT    = 0x08,
};

static bool is_option_set(LPCLI_OPTS *opts, unsigned flag)
{
	return (opts->flags & flag) ? true : false;
}

static void set_opt_length(LPCLI_OPTS *opts, unsigned value)
{
	opts->flags |= OPTS_LENGTH;
	opts->length = value;
}

static void set_opt_counter(LPCLI_OPTS *opts, unsigned value)
{
	opts->flags |= OPTS_COUNTER;
	opts->counter = value;
}

static void set_opt_charsets(LPCLI_OPTS *opts, unsigned flag)
{
	opts->flags |= OPTS_CHARSETS;
	opts->charsets |= flag;
}

static int read_args(int argc, char **argv, LPCLI_OPTS *opts)
{
	if (argc < 2)
		return LPCLI_FAIL;

	int i = 0;
	char *ptr;
	char *ptrEnd;
	ptr = argv[++i];
	opts->site = ptr;
	ptr = argv[++i]; // next

	opts->login = "";
	if (ptr && *ptr != '-')
	{
		opts->login = ptr;
		ptr = argv[++i]; // next
	}

	//if (ptr && *ptr != '-')
	//{
	//	opts->password = ptr;
	//	set_option(opts, OPTS_PASSWORD);
	//	ptr = argv[++i]; // next
	//}

	bool post_shopt = false; // is it just after a short option?

	while (argv[i])
	{
		if (!post_shopt)
		{
			if (*ptr != '-')
				return LPCLI_FAIL;
			ptr++;
		}

		switch (*ptr)
		{
		case '\0': // -
			if (!post_shopt)
				return LPCLI_FAIL;
			post_shopt = false; //stop short option reading
			ptr = argv[++i];
			break;
		case '-': // --
			if (post_shopt)
				return LPCLI_FAIL;
			ptr++;
			if (strcmp(ptr, "lowercase") == 0)
			{
				set_opt_charsets(opts, LP_CSF_LOWERCASE);
			}
			else if (strcmp(ptr, "uppercase") == 0)
			{
				set_opt_charsets(opts, LP_CSF_UPPERCASE);
			}
			else if (strcmp(ptr, "digits") == 0)
			{
				set_opt_charsets(opts, LP_CSF_DIGITS);
			}
			else if (strcmp(ptr, "symbols") == 0)
			{
				set_opt_charsets(opts, LP_CSF_SYMBOLS);
			}
			else if (strcmp(ptr, "print") == 0)
			{
				opts->flags |= OPTS_PRINT;
			}
			else if (strcmp(ptr, "length") == 0)
			{
				if (!(ptr = argv[++i]))
					return LPCLI_FAIL; //next!=null
				set_opt_length(opts, strtol(ptr, &ptrEnd, 10)); //FIXME: handle negative?
				if (*ptrEnd != '\0')
					return LPCLI_FAIL;
			}
			else if (strcmp(ptr, "counter") == 0)
			{
				if (!(ptr = argv[++i]))
					return LPCLI_FAIL; //next!=null
				set_opt_counter(opts, strtol(ptr, &ptrEnd, 10));
				if (*ptrEnd != '\0')
					return LPCLI_FAIL;
			}
			else
			{
				return LPCLI_FAIL;
			}
			ptr = argv[++i]; //next
			break;
		case 'n':
			ptr++;
			if (*ptr == '\0' && !(ptr = argv[++i]))
				return LPCLI_FAIL;
			set_opt_length(opts, strtol(ptr, &ptrEnd, 10));
			ptr = ptrEnd;
			post_shopt = true;
			break;
		case 'c':
			ptr++;
			if (*ptr == '\0' && !(ptr = argv[++i]))
				return LPCLI_FAIL;
			set_opt_counter(opts, strtol(ptr, &ptrEnd, 10));
			ptr = ptrEnd;
			post_shopt = true;
			break;
		case 'l':
			set_opt_charsets(opts, LP_CSF_LOWERCASE);
			ptr++;
			post_shopt = true;
			break;
		case 'u':
			set_opt_charsets(opts, LP_CSF_UPPERCASE);
			ptr++;
			post_shopt = true;
			break;
		case 'd':
			set_opt_charsets(opts, LP_CSF_DIGITS);
			ptr++;
			post_shopt = true;
			break;
		case 's':
			set_opt_charsets(opts, LP_CSF_SYMBOLS);
			ptr++;
			post_shopt = true;
			break;
		case 'p':
			opts->flags |= OPTS_PRINT;
			ptr++;
			post_shopt = true;
			break;
		default:
			return LPCLI_FAIL;
		}
	}
	return LPCLI_OK;
}

void print_options(LP_CTX *t)
{
	printf("Options: -");
	if (t->charsets & LP_CSF_LOWERCASE)
	{ printf("l"); }
	if (t->charsets & LP_CSF_UPPERCASE)
	{ printf("u"); }
	if (t->charsets & LP_CSF_DIGITS)
	{ printf("d"); }
	if (t->charsets & LP_CSF_SYMBOLS)
	{ printf("s"); }
	printf("c%u", t->counter);
	printf("n%u", t->length);
	printf("\n");
}

#ifndef zeromem
#define zeromem(dst,len) lpcli_zeromemory(dst,len)
#endif

int lpcli_main(int argc, char **argv)
{
	if (argc == 1 || (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)))
	{
		return print_usage();
	}

	LPCLI_OPTS options = {0};
	if (read_args(argc, argv, &options) != LPCLI_OK)
	{
		return print_error(errstr[ERR_OPTION]);
	}

	LP_CTX ctx;
	LP_CTX_init(&ctx);

	if (is_option_set(&options, OPTS_CHARSETS))
	{
		ctx.charsets = options.charsets;
	}

	if (is_option_set(&options, OPTS_LENGTH))
	{
		if (!LP_check_length(options.length))
		{
			return print_error(errstr[ERR_VALUE], "length", options.length);
		}
		ctx.length = options.length;
	}

	if (is_option_set(&options, OPTS_COUNTER))
	{
		if (!LP_check_counter(options.counter))
		{
			return print_error(errstr[ERR_VALUE], "counter", options.counter);
		}
		ctx.counter = options.counter;
	}

	print_options(&ctx);

	char passwd_in[LPMAXSTRLEN];
	if (lpcli_readpassword("Enter Password: ", passwd_in, sizeof passwd_in) != LPCLI_OK)
	{
		return print_error(errstr[ERR_PASSWORD]);
	}

	int ret = LP_generate(&ctx, options.site, options.login, (const char *) passwd_in);

    uint8_t passwd_hash[32];

    hmac_sha256(passwd_in, "", passwd_hash);
	zeromem(passwd_in, sizeof passwd_in); // clean password read

	if (ret < 1)
	{
		// zeromem(&ctx, sizeof ctx); //no need
		return print_error(errstr[ERR_GENERATE], ret);
	}

    long unsigned int offset = 0;

    for (int i = 0; i < 9; i++) {
        offset = (offset << 8) + passwd_hash[i];

        if ((i + 1) % 3 == 0) {
            printf(" %s%s", getColor(offset), getIcon(offset));
            offset = 0;
        }
    }

    printf("\033[0m\n");

	bool clipboardcopy = is_option_set(&options, OPTS_PRINT) ? false : true;

	if (clipboardcopy)
	{
		if (lpcli_clipboardcopy(ctx.buffer) != LPCLI_OK)
		{ return print_error(errstr[ERR_CLIPBOARD]); }
	}
	else
	{

		printf("%s\n", ctx.buffer);
	}

	zeromem(&ctx, sizeof ctx); // clean generated password
	return LPCLI_OK;
}

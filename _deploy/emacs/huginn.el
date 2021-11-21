(require 'cc-mode)
(require 'newcomment)
(require 'thingatpt)
(require 'imenu)
(eval-when-compile
	(require 'cl-lib)
	(require 'ido))

(setq huginn-font-lock-keywords
	'(
		( "\\<[A-Z][a-zA-Z]+\\>" . 'font-lock-user-defined-classes-face )
		( "\\<_[a-zA-Z0-9]+\\>" . 'font-lock-field-face )
		( "\\<[a-zA-Z0-9]+_\\>" . 'font-lock-argument-face )
		( "\\<[A-Z][A-Z0-9_]+\\>" . 'font-lock-const-face )
		( "\\<\\(assert\\|break\\|case\\|catch\\|class\\|constructor\\|continue\\|default\\|destructor\\|else\\|enum\\|for\\|if\\|return\\|super\\|switch\\|this\\|throw\\|try\\|while\\)\\>" . 'font-lock-keyword-face )
		( "\\<\\(blob\\|boolean\\|character\\|integer\\|number\\|real\\|string\\|tuple\\|list\\|deque\\|dict\\|lookup\\|order\\|set\\|type\\|size\\|copy\\|observe\\|use\\)\\>" . 'font-lock-builtin-face )
		( "\\<\\(import\\|as\\|from\\)\\>" . 'font-lock-preprocessor-face )
		( "\\(\\<[0-9]+\\>\\|\\.[0-9]+\\>\\|\\<[0-9]+\\.\\|\\<[0-9]+\\.[0-9]+\\>\\)" . 'font-lock-number-face )
								; more of those would go here
	)
)

(define-derived-mode huginn-mode prog-mode "Huginn"
	"Major mode for editing Huginn programming language source code."
	:group 'huginn
	:syntax-table c++-mode-syntax-table
	(modify-syntax-entry ?_ "w" c++-mode-syntax-table)
	(setq-local font-lock-defaults '(huginn-font-lock-keywords))
	(funcall 'highlight-escapes)
)

(setq auto-mode-alist (cons '("\\.hgn" . huginn-mode) auto-mode-alist))
(add-to-list 'interpreter-mode-alist '(".*\n.*\n.*huginn" . huginn-mode))
(add-to-list 'magic-mode-alist '(".*\n.*\n#!.*huginn" . huginn-mode))

(provide 'huginn)


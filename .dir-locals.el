((c-mode . ((mode . c++)
            (eval . (setq flycheck-gcc-include-path
                          '("../"
                            "../include"
                            "../../include"
                            "../submodules/googletest/googletest/include"
                            "../../../include")))
            (eval . (c-set-offset 'innamespace 0))))
 (c++-mode . ((eval . (setq flycheck-gcc-include-path
                            '("../"
                              "../include"
                              "../../include"
                              "../submodules/googletest/googletest/include"
                              "../../../include")))
              (eval . (c-set-offset 'innamespace 0))
              (flycheck-gcc-language-standard . "gnu++17"))))

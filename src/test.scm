; this is a file with code in it

(def map (lst proc)
     (if (null? lst) f
         (do
             (proc (car lst))
             (map (cdr lst) proc))))

(def prn (args ...)
     (map args pr)
     (pr "\n"))

(defmacro and
  (fn (args ...)
      (if (null? args)
          t
          (if (== (len args) 1)
              (car args)
              `(if ,(car args)
                   (and ,@(cdr args))
                   f)))))

(if (and t 1)
     (prn "they are both true!"))

(prn "hello")
(prn (+ 3 5))
(prn '(+ 3 5))

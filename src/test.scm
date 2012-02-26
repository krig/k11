; implementing some basic operations...

(def map (lst proc)
     (if (null? lst) f
         (do
             (proc (car lst))
             (map (cdr lst) proc))))

(def prn (args ...)
     (let (ret (map args pr))
       (pr "\n")
       ret))

(defmacro and
  (fn (args ...)
      (if (null? args)
          t
          (if (== (len args) 1)
              (car args)
              `(if ,(car args)
                   (and ,@(cdr args))
                   f)))))

(defmacro or
  (fn (args ...)
      (if (null? args)
          t
          (if (== (len args) 1)
              (car args)
              `(if (not ,(car args))
                   (or ,@(cdr args))
                   t)))))

(defmacro cond
  (fn (args ...)
      (if (null? args)
          t
          (let (pred (car args)
                action (car (cdr args))
                rest (cdr (cdr args)))
            `(if ,pred ,action
                 (cond ,@rest))))))

(def fact (x)
     (if (== x 1)
         1
         (* x (fact (- x 1)))))

(def reverse (lst)
     (if (null? lst) lst
         (append (reverse (cdr lst)) (list (car lst)))))

(def member (value lst)
     (if (null? lst) f
         (if (== value (car lst))
             lst
             (member value (cdr lst)))))

(def range-rec (n lst)
     (if (== n 0)
         lst
         (range-rec (- n 1) (cons n lst))))

(def range (n)
     (range-rec n '()))



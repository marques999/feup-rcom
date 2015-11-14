(require (lib "audio.scm" "user-feup"))
(define diz-numero
  (lambda (numero)
     ;1. Determinar milhares e unidades
    (let (
          (milhares (quotient numero 1000))
          (unidades (remainder numero 1000))
          )
      ;2. Dizer milhares
      (if (> milhares 0)
          (begin
          (if (> milhares 1)
              (dizer-num-3-digitos milhares))
          
          (som "1000")
          
          (if (and 
               (positive? unidades)
               (or (< unidades 100) 
                       (zero? (remainder unidades 100))
               )
               )
              (som "e"))
           )
       )
      ;3. Dizer unidades
      (if (or (positive? unidades) (zero? numero))
          (dizer-num-3-digitos unidades))
      
      )))
(define dizer-num-3-digitos
  (lambda (numero)
    (let (
          (c (quotient numero 100))
          (d (remainder (quotient numero 10) 10))
          (u (remainder numero 10))
          
          ;Dizer as centenas do número
          (dizer-centenas numero)
          ;Se necessário, Liga centenas com dezenas
          (if (and (> c 0) (> d 0))
              (som "e"))
          ;Dizer as dezenas do número
          (dizer-dezenas numero)
          ;Se não fior caso de 10 a 19
          (if (not (= d 1))
              (begin
                (if (and 
                     (or (> c 0) (> d 0))
                     (> u 0))
                    (som "e"))
                (dizer-unidades numero))))
      )
    )
  )

(define dizer-centenas
  (lambda (numero)
  (if (= numero 100)
      (som "100")
      (let ((c (quotient numero 100)))
        (cond
          ((= c 9)(som "900"))
          ((= c 8)(som "800"))
          ((= c 7)(som "700"))
          ((= c 6)(som "600"))
          ((= c 5)(som "500"))
          ((= c 4)(som "400"))
          ((= c 3)(som "300"))
          ((= c 2)(som "200"))
          ((= c 1)(som "c100")))))))
(define dizer-dezenas
  (lambda (numero)
    (let (
          (d (remainder (quotient numero 10) 10))
          (u (remainder numero 10))
          )
      (if (= d 1)
      (cond
          ((= u 9)(som "19"))
          ((= u 8)(som "18"))
          ((= u 7)(som "17"))
          ((= u 6)(som "16"))
          ((= u 5)(som "15"))
          ((= u 4)(som "14"))
          ((= u 3)(som "13"))
          ((= u 2)(som "12"))
          ((= u 1)(som "11")))
      ;Se não, temos os casos que concatenam com as unidades: 20,30...90~
      ;Se o dígito das dezernas for 2, 3, ... ou 9
      ;Assim dirá "20", "30", ... ou "90".
      (cond
        ((= d 2)(som "20"))
        ((= d 3)(som "30"))
        ((= d 4)(som "40"))
        ((= d 5)(som "50"))
        ((= d 6)(som "60"))
        ((= d 7)(som "70"))
        ((= d 8)(som "80"))
        ((= d 9)(som "90"))
      ) 
    )
  )))
      
function sum(from, to)
   if from == to then
        return from;
   else
        local prev = sum(from + 1, to);
        return from + prev;
   end
end

function what_if(n)
  if n > 0 then
     return sum(n, n+100);
  else
     if n == 0 then
        return -1;
     else
        return -2;
     end
  end   
end

print(what_if(1));
print(what_if(0));
print(what_if(-1));
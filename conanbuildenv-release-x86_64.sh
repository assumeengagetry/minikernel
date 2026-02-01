script_folder="/home/assumeengage/Kernal"
echo "echo Restoring environment" > "$script_folder/deactivate_conanbuildenv-release-x86_64.sh"
for v in PATH
do
   is_defined="true"
   value=$(printenv $v) || is_defined="" || true
   if [ -n "$value" ] || [ -n "$is_defined" ]
   then
       echo export "$v='$value'" >> "$script_folder/deactivate_conanbuildenv-release-x86_64.sh"
   else
       echo unset $v >> "$script_folder/deactivate_conanbuildenv-release-x86_64.sh"
   fi
done

export PATH="/home/assumeengage/.conan2/p/meson85908f9d185b9/f/bin:/home/assumeengage/.conan2/p/ninja7c42328b4bc42/p/bin:$PATH"